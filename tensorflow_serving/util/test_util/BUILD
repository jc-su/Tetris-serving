# Description: testing utils for Tensorflow Serving utils.

package(
    default_visibility = [
        "//tensorflow_serving:internal",
    ],
    features = ["-layering_check"],
)

licenses(["notice"])

cc_library(
    name = "mock_file_probing_env",
    testonly = 1,
    hdrs = ["mock_file_probing_env.h"],
    deps = [
        "//tensorflow_serving/util:file_probing_env",
        "@com_google_googletest//:gtest",
        "@org_tensorflow//tensorflow/core:lib",
    ],
)
