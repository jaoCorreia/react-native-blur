require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name            = "react-native-blur"
  s.version         = package["version"]
  s.summary         = package["description"]
  s.homepage        = package["repository"]["url"]
  s.license         = package["license"]
  s.author          = package["author"]
  s.platforms       = { :ios => "13.4" }
  s.source          = { :git => package["repository"]["url"], :tag => "v#{s.version}" }

  s.source_files    = "blur-core/include/**/*.h", "blur-core/src/**/*.cpp"
  s.public_header_files = "blur-core/include/**/*.h"

  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++17",
    "HEADER_SEARCH_PATHS" => "$(PODS_TARGET_SRCROOT)/blur-core/include",
    "OTHER_CFLAGS" => "-fno-omit-frame-pointer"
  }

  s.dependency "React-Core"
end
