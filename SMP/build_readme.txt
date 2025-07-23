linux x86 and arm build

./configure \
--enable-static --enable-shared --disable-asm \
--disable-lavf	--disable-swscale \
--prefix=/1078/MediaProject/linux_build

PKG_CONFIG_PATH=/1078/MediaProject/linux_build/lib/pkgconfig ./configure \
--prefix=/1078/MediaProject/linux_build \
--enable-gpl --enable-version3 --enable-nonfree \
--disable-debug --enable-optimizations --enable-small \
--enable-shared --enable-static \
--enable-libx264 --disable-libx265 \
--disable-libfdk-aac --disable-libmp3lame --disable-libopus --disable-libvorbis \
--disable-libfribidi --disable-libfreetype --disable-libvpx --disable-libtheora \
--disable-libvpx \
--disable-libxvid --disable-libwebp --disable-libzimg --disable-libass \
--extra-cflags=-I/1078/MediaProject/linux_build/include \
--extra-ldflags="-L/1078/MediaProject/linux_build/lib " 	\
--pkg-config=pkg-config




./configure \
--host=aarch64-linux-gnu --cross-prefix=aarch64-linux-gnu- \
--enable-static --enable-shared --disable-asm \
--disable-lavf	--disable-swscale \
--prefix=/1078/MediaProject/linux_build_arm


PKG_CONFIG_PATH=/1078/MediaProject/linux_build_arm/lib/pkgconfig ./configure \
--cross-prefix=aarch64-linux-gnu- --arch=arm64 --target-os=linux \
--cc=aarch64-linux-gnu-gcc --cxx=aarch64-linux-gnu-g++ \
--prefix=/1078/MediaProject/linux_build_arm \
--enable-gpl --enable-version3 --enable-nonfree \
--disable-debug --enable-optimizations --enable-small \
--enable-shared --enable-static \
--enable-libx264 --disable-libx265 \
--disable-libfdk-aac --disable-libmp3lame --disable-libopus --disable-libvorbis \
--disable-libfribidi --disable-libfreetype --disable-libvpx --disable-libtheora \
--disable-libvpx \
--disable-libxvid --disable-libwebp --disable-libzimg --disable-libass \
--extra-cflags=-I/1078/MediaProject/linux_build_arm/include \
--extra-ldflags="-L/1078/MediaProject/linux_build_arm/lib " 	\
--pkg-config=pkg-config

ffmpeg -i ../input.mp4 -q:v 2 output%d.jpg