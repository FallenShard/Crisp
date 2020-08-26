outputDir="../../../Resources/Shaders/"
mkdir -p $outputDir

echo "Compiling GLSL shaders into SPIR-V..."

errorPrefix="ERROR"

for entry in *.glsl; do
    # grab the last modified timestamp of the input shader, in epoch seconds
    inputModified=$(date +%s -r $entry)

    # extracts the shader type from the name - {vert, frag, tese, tesc, geom, comp}
    shaderType="${entry##*-}"
    shaderType=${shaderType:0:4}

    # extracts the shader name e.g. "my-shader"
    techniqueName="${entry%-*}"

    # creates the name for the output file with spv extension e.g. "dir/my-shader-vert.spv"
    output=$outputDir$techniqueName"-"$shaderType".spv"

    # grab the last modified timestamp of the output if it exists, in epoch seconds
    outputModified=0
    if [ -e "$output" ]; then
        outputModified=$(date +%s -r $output)
    fi

    # If input was modified later than output, overwrite output with a freshly compiled version
    if [[ "$inputModified" > "$outputModified" ]]; then
        printf "Time to write!\n"

        if [ -e "$output" ]; then
            rm "$output"
        fi

        maxMessageLength=60
        message=$(printf "Compiling %q to SPIR-V" "$entry")
        currLen=${#message}

        suffixLen=$(($maxMessageLength-$currLen))
        ch='.'
        suffix=$(printf '%*s' $suffixLen | tr ' ' $ch)
        message=$message$suffix

        printf "%s" "$message"

        compileOutput=$(glslangValidator.exe -V -o $output -S $shaderType "$entry")
        if [[ $compileOutput =~ $errorPrefix || $1 = "-v" ]]; then
            printf "\n%s\n" "$compileOutput"
        else
            printf "Success!\n"
        fi
    fi
done

echo "Done."