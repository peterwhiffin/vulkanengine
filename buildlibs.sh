# clang++ -c ../../inc/imgui/*.cpp -I../../inc -I$VULKAN_SDK/include
# ar rcs libimgui.a *.o

clang++ -c -o lib/lin/vma/vma.o src/vma_impl.cpp -I../../inc -I"$VULKAN_SDK/include" -I"../VulkanMemoryAllocator/include/"
ar rcs lib/lin/vma/libvma.a lib/lin/vma/vma.o

pushd lib/lin/imgui || exit
clang++ -c ../../../inc/imgui/*.cpp -I../../../../volk -I"$VULKAN_SDK/include"
ar rcs libimgui.a *.o
popd || exit
