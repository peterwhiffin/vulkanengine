./cmpshaders.sh

NAME="vulkanengine"
BUILD_DIR="./build/lin"
OUT="${BUILD_DIR}/${NAME}"

COMPILER="clang"

SRC=(
	src/main.c
)

FLAGS=(
	-std=c23
	-g
	# -Wall
)

DEFINES=(
	-DLOG_LVL=3
)

INCLUDES=(
	-I"./inc"
	-I"$VULKAN_SDK/include"
	-I"../VulkanMemoryAllocator/include"
	-I"../volk"
	-I"../cglm/include"
)

LIB_PATHS=(
	-L"./lib/lin"
	-L"./lib/lin/vma"
	-L"./lib/lin/imgui"
	-L"$VULKAN_SDK/lib"
)

LIBS=(
	-lpthread
	-lSDL3
	-lm
	-lslang
	-lvma
	-limgui
	-lstdc++
)

"${COMPILER}" "${FLAGS[@]}" -o "${OUT}" "${SRC[@]}" "${DEFINES[@]}" "${INCLUDES[@]}" "${LIB_PATHS[@]}" "${LIBS[@]}"
