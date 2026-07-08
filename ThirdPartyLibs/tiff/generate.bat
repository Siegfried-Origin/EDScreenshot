set ROOT=%CD%
set DEPS=%ROOT%\deps

git clone https://github.com/madler/zlib

cd zlib
cmake -B build -A x64 ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_INSTALL_PREFIX=%DEPS% ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>"

cmake --build build --config Release
cmake --install build --config Release

cd ..

git clone https://github.com/libsdl-org/libtiff

cd libtiff
cmake -B build -A x64 ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DZLIB_USE_STATIC_LIBS=ON ^
  -DCMAKE_PREFIX_PATH=%DEPS% ^
  -DCMAKE_INSTALL_PREFIX=%DEPS% ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" ^
  -Djpeg=OFF ^
  -Dwebp=OFF ^
  -Dzstd=OFF ^
  -Dlzma=OFF ^
  -Djbig=OFF

cmake --build build --config Release
cmake --install build --config Release