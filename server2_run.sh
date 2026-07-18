cmake -S . -B build -DBUILD_CLIENT=OFF \
                    -DBUILD_SERVER=ON \
                    -DBUILD_SERVER2=ON
cmake --build build-server2 -j
./build-server2/server2/CloudMeetingAuthServer