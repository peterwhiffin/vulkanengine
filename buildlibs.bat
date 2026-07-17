REM clang++ -c -o lib/lin/vma/vma.o src/vma_impl.cpp -I../../inc -I"$VULKAN_SDK/include" -I"../VulkanMemoryAllocator/include/"
REM ar rcs lib/lin/vma/libvma.a lib/lin/vma/vma.o
cl /c /Folib\win\vma.obj /I"../VulkanMemoryAllocator/include/" /I"X:/vulkanSDK/Include/" src/vma_impl.cpp
lib /OUT:lib\win\vma.lib lib\win\vma.obj
