# Build rednodelabs/otbr:base docker

docker buildx build --platform linux/amd64,linux/arm/v7 --build-arg WEB_GUI=0 --build-arg REST_API=0 --build-arg BACKBONE_ROUTER=0 --build-arg OTBR_OPTIONS='-DOTBR_BACKBONE_ROUTER=OFF -DOT_THREAD_VERSION=1.1' -t rednodelabs/otbr:base-0.9.6 -f etc/docker/Dockerfile --push .

## Local build
BACKBONE_ROUTER=OFF THREAD_VERSION=1.1 ./script/localbuild

./build/otbr/src/agent/otbr-agent -I wpan0 -v 'spinel+hdlc+uart:///dev/ttyACM0?uart-baudrate=1000000' -d6
