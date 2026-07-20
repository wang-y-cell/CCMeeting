cmake -S . -B build -DBUILD_CLIENT=OFF \
                    -DBUILD_SERVER=ON \
                    -DBUILD_SERVER2=ON
cmake --build build-server -j
./build-server/server/CloudMeetingServer