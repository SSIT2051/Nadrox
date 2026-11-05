# CMake generated Testfile for 
# Source directory: /home/ssit/Projects/Nadrox/thirdparty/wolfssl
# Build directory: /home/ssit/Projects/Nadrox/build_test/thirdparty/wolfssl
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_test "/home/ssit/Projects/Nadrox/build_test/thirdparty/wolfssl/tests/unit.test")
set_tests_properties(unit_test PROPERTIES  WORKING_DIRECTORY "/home/ssit/Projects/Nadrox/thirdparty/wolfssl" _BACKTRACE_TRIPLES "/home/ssit/Projects/Nadrox/thirdparty/wolfssl/CMakeLists.txt;2816;add_test;/home/ssit/Projects/Nadrox/thirdparty/wolfssl/CMakeLists.txt;0;")
add_test(wolfcrypttest "/home/ssit/Projects/Nadrox/build_test/thirdparty/wolfssl/wolfcrypt/test/testwolfcrypt")
set_tests_properties(wolfcrypttest PROPERTIES  WORKING_DIRECTORY "/home/ssit/Projects/Nadrox/thirdparty/wolfssl" _BACKTRACE_TRIPLES "/home/ssit/Projects/Nadrox/thirdparty/wolfssl/CMakeLists.txt;2858;add_test;/home/ssit/Projects/Nadrox/thirdparty/wolfssl/CMakeLists.txt;0;")
