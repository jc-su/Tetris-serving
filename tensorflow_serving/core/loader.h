/* Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_SERVING_CORE_LOADER_H_
#define TENSORFLOW_SERVING_CORE_LOADER_H_

#include <memory>

#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow_serving/core/source.h"
#include "tensorflow_serving/resources/resources.pb.h"
#include "tensorflow_serving/util/any_ptr.h"

namespace tensorflow {
namespace serving {

/// A standardized abstraction for an object that manages the lifecycle of a
/// servable, including loading and unloading it. Servables are arbitrary
/// objects that serve algorithms or data that often, though not necessarily,
/// use a machine-learned model.
///
/// A Loader for a servable object represents one instance of a stream of
/// servable versions, all sharing a common name (e.g. "my_servable") and
/// increasing version numbers, typically representing updated model parameters
/// learned from fresh training data.
///
/// A Loader should start in an unloaded state, meaning that no work has been
/// done to prepare to perform operations. A typical instance that has not yet
/// been loaded contains merely a pointer to a location from which its data can
/// be loaded (e.g. a file-system path or network location). Construction and
/// destruction of instances should be fairly cheap. Expensive initialization
/// operations should be done in Load().
///
/// Subclasses may optionally store a pointer to the Source that originated it,
/// for accessing state shared across multiple servable objects in a given
/// servable stream.
///
/// Implementations need to ensure that the methods they expose are thread-safe,
/// or carefully document and/or coordinate their thread-safety properties with
/// their clients to ensure correctness.
/// Servables do not need to worry about concurrent execution of Load()/Unload()
/// as the caller will ensure that does not happen.
class Loader {
 public:
  /// The destructor will never be called on a Loader whose servable is
  /// currently loaded, i.e. between (successful) calls to Load() and Unload().
  virtual ~Loader() = default;

  /// Estimates the resources a servable will use.
  ///
  /// IMPORTANT: This method's implementation must obey following requirements,
  /// which enable the serving system to reason correctly about which servables
  /// can be loaded safely:
  ///  1. The estimate must represent an upper bound on the actual value.
  ///  2. Prior to load, the estimate may include resources that are not bound
  ///     to any specific device instance, e.g. RAM on one of the two GPUs.
  ///  3. While loaded, for any devices with multiple instances (e.g. two GPUs),
  ///     the estimate must specify the instance to which each resource is
  ///     bound.
  ///  4. The estimate must be monotonically non-increasing, i.e. it cannot
  ///     increase over time. Reasons to have it potentially decrease over time
  //      include: (a) replace conservative estimate with actual measurement
  //      once loaded in memory; (b) load process consumes extra transient
  //      memory that is not used in steady-state after the load completes.
  ///
  /// @return an estimate of the resources the servable will consume once
  /// loaded. If the servable has already been loaded, returns an estimate of
  /// the actual resource usage.
  virtual Status EstimateResources(ResourceAllocation* estimate) const = 0;

  /// Fetches any data that needs to be loaded before using the servable
  /// returned by servable(). May use no more resources than the estimate
  /// reported by EstimateResources().
  ///
  /// If implementing Load(), you don't have to override LoadWithMetadata().
  virtual Status Load() {
    return errors::Unimplemented("Load isn't implemented.");
  }

  /// The metadata consists of the ServableId.
  struct Metadata {
    ServableId servable_id;
  };
  /// Similar to the above method, but takes Metadata as a param, which
  /// may be used by the loader implementation appropriately.
  ///
  /// If you're overriding LoadWithMetadata(), because you can use the metadata
  /// appropriately, you can skip overriding Load().
  virtual Status LoadWithMetadata(const Metadata& metadata) { return Load(); }

  /// Frees any resources allocated during Load() (except perhaps for resources
  /// shared across servables that are still needed for other active ones).
  /// The loader does not need to return to the "new" state (i.e. Load() cannot
  /// be called after Unload()).
  virtual void Unload() = 0;

  /// Returns an opaque interface to the underlying servable object.
  /// The caller should know the precise type of the interface in order to make
  /// actual use of it. For example:
  ///
  /// CustomLoader implementation:
  ///
  ///     class CustomLoader : public Loader {
  ///      public:
  ///       ...
  ///       Status Load() override {
  ///         servable_ = ...;
  ///       }
  ///
  ///       AnyPtr servable() override { return servable_; }
  ///
  ///      private:
  ///       CustomServable* servable_ = nullptr;
  ///     };
  ///
  /// Serving user request:
  ///
  ///     ServableHandle&lt;CustomServable> handle = ...
  ///     CustomServable* servable = handle.get();
  ///     servable->...
  ///
  /// If servable() is called after successful Load() and before Unload(), it
  /// returns a valid, non-null AnyPtr object. If called before a successful
  /// Load() call or after Unload(), it returns null AnyPtr.
  virtual AnyPtr servable() = 0;
};

inline bool operator==(const Loader::Metadata& a, const Loader::Metadata& b) {
  return a.servable_id == b.servable_id;
}

inline bool operator!=(const Loader::Metadata& a, const Loader::Metadata& b) {
  return !(a == b);
}

/// A Loader that is oblivious to resources. Its EstimateResources() method
/// returns 0, thus effectively disabling resource-based safety checks in the
/// serving system.
///
/// Loaders that are experimental, or run in environments that do not need the
/// resource safety checks, can subclass ResourceUnsafeLoader instead of Loader.
class ResourceUnsafeLoader : public Loader {
 public:
  Status EstimateResources(ResourceAllocation* estimate) const final {
    estimate->Clear();
    return Status();
  }
};

// A source that emits Loader unique pointers.
using LoaderSource = Source<std::unique_ptr<Loader>>;

}  // namespace serving
}  // namespace tensorflow

#endif  // TENSORFLOW_SERVING_CORE_LOADER_H_
