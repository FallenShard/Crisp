mkdir -p "../../Resources/Shaders"

rm -r ../../Resources/Shaders/*

errorPrefix="ERROR"

for entry in *.glsl;
do
    shaderType=$(echo "$entry" | grep -o '\-[a-z]*\.')
    shaderType=${shaderType:1:4}
    techniqueName=$(echo "$entry" | grep -o '.*\.')
    techniqueName=${techniqueName:0:${#techniqueName}-6}
    output="../../Resources/Shaders/"$techniqueName"-"$shaderType".spv"

    maxMessageLength=60
    message=$(printf "Compiling %q to SPIR-V" "$entry")
    currLen=${#message}

    suffixLen=$(($maxMessageLength-$currLen))
    ch='.'
    suffix=$(printf '%*s' $suffixLen | tr ' ' $ch)
    message=$message$suffix

    printf "%s" "$message"

    compileOutput=$(glslangValidator.exe -V -o $output -S $shaderType "$entry")

    if [[ $compileOutput =~ $errorPrefix || $1 = "-v" ]]
    then
        printf "\n%s\n" "$compileOutput"
    else
        printf "Success!\n"
    fi
done