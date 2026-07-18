cmake -S . -B build-server -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON -DBUILD_SERVER2=OFF
cmake --build build-server -j
./build-server/server/CloudMeetingServer