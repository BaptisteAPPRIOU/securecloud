Commandes utiles

Bootstrap vcpkg (à la racine du repo):

git submodule add https://github.com/microsoft/vcpkg.git tools/vcpkg || true
./tools/vcpkg/bootstrap-vcpkg.sh  # .bat sous Windows

Build local du gateway :

cmake --preset dev -S gateway
cmake --build gateway/build -j
./gateway/build/gateway

Docker :

docker build -t gateway:dev -f ops/docker/gateway.Dockerfile .
docker run --rm -p 8443:8443 gateway:dev