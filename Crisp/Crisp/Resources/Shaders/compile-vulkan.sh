mkdir -p "Vulkan"

rm -r Vulkan/*

for entry in *.glsl;
do
    shaderType=$(echo "$entry" | grep -o '\-[a-z]*\.')
    shaderType=${shaderType:1:4}
    techniqueName=$(echo "$entry" | grep -o '.*\.')
    techniqueName=${techniqueName:0:${#techniqueName}-6}
    output="Vulkan/"$techniqueName"-"$shaderType".spv"

    glslangValidator.exe -V -o $output -S $shaderType "$entry"

    printf "\n"
done