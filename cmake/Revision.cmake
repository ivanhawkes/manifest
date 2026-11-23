# Run at every build (source/game/host): writes OUT from IN with the trees the build is made from, so
# that two builds whose code differs never share an identity (D-111), a build from uncommitted
# changes included. A tree is git's own hash of a checkout's build inputs as they are on disk,
# tracked or not, taken in an index and an object store of the build tree's: the checkout's own are
# not touched, and the index kept between builds means only changed files are read again. OUT is
# rewritten only when a tree has changed. GAME and SDK are directories anywhere inside each checkout.
function(manifest_tree out checkout name)
    set(git "${CMAKE_COMMAND}" -E env "GIT_INDEX_FILE=${WORK}/${name}.index"
        "GIT_OBJECT_DIRECTORY=${WORK}/${name}.objects" "${GIT}" -C "${checkout}")
    file(MAKE_DIRECTORY "${WORK}/${name}.objects")
    execute_process(COMMAND ${git} add --all -- :/source :/cmake :/CMakeLists.txt :/CMakePresets.json
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND ${git} write-tree
        OUTPUT_VARIABLE tree OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    set(${out} "${tree}" PARENT_SCOPE)
endfunction()

manifest_tree(game "${GAME}" game)
manifest_tree(sdk "${SDK}" sdk)
configure_file("${IN}" "${OUT}" @ONLY)
