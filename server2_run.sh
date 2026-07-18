cmake -S . -B build-server2 -DBUILD_CLIENT=OFF -DBUILD_SERVER=OFF -DBUILD_SERVER2=ON
cmake --build build-server2 -j
./build-server2/server2/CloudMeetingAuthServer