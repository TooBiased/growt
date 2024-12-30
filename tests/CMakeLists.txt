function( GrowTExecutable variant cpp directory name )
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
  add_executable(${name} ${cpp}.cpp)
  set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
  target_compile_definitions(${name} PRIVATE
    -D ${variant}
    -D ${GROWT_HASHFCT}
    -D GROWT_USE_CONFIG)
  target_link_libraries(${name} PRIVATE Threads::Threads)
endfunction( GrowTExecutable )

GrowTExecutable( UAGROW functionality fun functionality_uaGrowT )
GrowTExecutable( USGROW functionality fun functionality_usGrowT )
GrowTExecutable( PAGROW functionality fun functionality_paGrowT )
GrowTExecutable( PSGROW functionality fun functionality_psGrowT )

GrowTExecutable( FOLKLORE ins_test ins ins_none_folklore )
GrowTExecutable( FOLKLORE mix_test mix mix_none_folklore )
GrowTExecutable( FOLKLORE con_test con con_none_folklore )
GrowTExecutable( FOLKLORE agg_test agg agg_none_folklore )

GrowTExecutable( SEQUENTIAL ins_test seq ins_full_sequential )
GrowTExecutable( SEQUENTIAL mix_test seq mix_full_sequential )
GrowTExecutable( SEQUENTIAL con_test seq con_full_sequential )
GrowTExecutable( SEQUENTIAL agg_test seq agg_full_sequential )

GrowTExecutable( UAGROW ins_test ins ins_full_uaGrowT )
GrowTExecutable( USGROW ins_test ins ins_full_usGrowT )
GrowTExecutable( PAGROW ins_test ins ins_full_paGrowT )
GrowTExecutable( PSGROW ins_test ins ins_full_psGrowT )
GrowTExecutable( UAGROW mix_test mix mix_full_uaGrowT )
GrowTExecutable( USGROW mix_test mix mix_full_usGrowT )
GrowTExecutable( PAGROW mix_test mix mix_full_paGrowT )
GrowTExecutable( PSGROW mix_test mix mix_full_psGrowT )
GrowTExecutable( UAGROW del_test del del_full_uaGrowT )
GrowTExecutable( USGROW del_test del del_full_usGrowT )
GrowTExecutable( PAGROW del_test del del_full_paGrowT )
GrowTExecutable( PSGROW del_test del del_full_psGrowT )
GrowTExecutable( UAGROW con_test con con_full_uaGrowT )
GrowTExecutable( USGROW con_test con con_full_usGrowT )
GrowTExecutable( PAGROW con_test con con_full_paGrowT )
GrowTExecutable( PSGROW con_test con con_full_psGrowT )
GrowTExecutable( UAGROW agg_test agg agg_full_uaGrowT )
GrowTExecutable( USGROW agg_test agg agg_full_usGrowT )
GrowTExecutable( PAGROW agg_test agg agg_full_paGrowT )
GrowTExecutable( PSGROW agg_test agg agg_full_psGrowT )

GrowTExecutable( FOLKLORE ins32_test  ins32  ins32_none_folklore )
GrowTExecutable( UAGROW   ins32_test  ins32  ins32_full_uaGrowT )


if (GROWT_BUILD_MEMORY_TEST)
  GrowTExecutable( FOLKLORE ins_test mem mem_none_folklore )
  target_compile_definitions(mem_none_folklore PRIVATE -D GROWT_RSS_MODE)
  GrowTExecutable( UAGROW ins_test mem mem_full_uaGrowT )
  target_compile_definitions(mem_full_uaGrowT PRIVATE -D GROWT_RSS_MODE)
  GrowTExecutable( USGROW ins_test mem mem_full_usGrowT )
  target_compile_definitions(mem_full_usGrowT PRIVATE -D GROWT_RSS_MODE)
endif()


add_custom_target( folklore )
add_dependencies( folklore
  ins_none_folklore mix_none_folklore con_none_folklore
  agg_none_folklore)

add_custom_target( sequential )
add_dependencies( sequential
  ins_full_sequential mix_full_sequential con_full_sequential
  agg_full_sequential)

add_custom_target( uagrow )
add_dependencies( uagrow
  ins_full_uaGrowT mix_full_uaGrowT con_full_uaGrowT
  agg_full_uaGrowT del_full_uaGrowT)

add_custom_target( usgrow )
add_dependencies( usgrow
  ins_full_usGrowT mix_full_usGrowT con_full_usGrowT
  agg_full_usGrowT del_full_usGrowT)

add_custom_target( pagrow )
add_dependencies( pagrow
  ins_full_paGrowT mix_full_paGrowT con_full_paGrowT
  agg_full_paGrowT del_full_paGrowT)

add_custom_target( psgrow )
add_dependencies( psgrow
  ins_full_psGrowT mix_full_psGrowT con_full_psGrowT
  agg_full_psGrowT del_full_psGrowT)

add_custom_target( functionality )
add_dependencies ( functionality
  functionality_uaGrowT
  functionality_usGrowT
  functionality_paGrowT
  functionality_psGrowT)

add_custom_target( ins )
add_dependencies( ins
  ins_full_sequential
  ins_none_folklore
  ins_full_uaGrowT    ins_full_usGrowT
  ins_full_uaGrowT    ins_full_usGrowT)

add_custom_target( agg )
add_dependencies( agg
  agg_full_sequential
  agg_none_folklore
  agg_full_uaGrowT    agg_full_usGrowT
  agg_full_uaGrowT    agg_full_usGrowT)

add_custom_target( mix )
add_dependencies( mix
  mix_full_sequential
  mix_none_folklore
  mix_full_uaGrowT    mix_full_usGrowT
  mix_full_uaGrowT    mix_full_usGrowT)

add_custom_target( con )
add_dependencies( con
  con_full_sequential
  con_none_folklore
  con_full_uaGrowT    con_full_usGrowT
  con_full_uaGrowT    con_full_usGrowT)

add_custom_target( del )
add_dependencies( del
  del_full_uaGrowT    del_full_usGrowT
  del_full_uaGrowT    del_full_usGrowT)

if (GROWT_BUILD_CIRCULAR)
  GrowTExecutable( UAGROW ins_test ins ins_full_cmuaGrowT )
  target_compile_definitions(ins_full_cmuaGrowT PRIVATE
    -D CMAP)
  GrowTExecutable( USGROW ins_test ins ins_full_cmusGrowT )
  target_compile_definitions(ins_full_cmusGrowT PRIVATE
    -D CMAP)
  GrowTExecutable( UAGROW ins_test ins ins_full_cpuaGrowT )
  target_compile_definitions(ins_full_cpuaGrowT PRIVATE
    -D CPROB)
  GrowTExecutable( USGROW ins_test ins ins_full_cpusGrowT )
  target_compile_definitions(ins_full_cpusGrowT PRIVATE
    -D CPROB)
  GrowTExecutable( UAGROW ins_test ins ins_full_ccuaGrowT )
  target_compile_definitions(ins_full_ccuaGrowT PRIVATE
    -D CMAP -D CPROB)
  GrowTExecutable( USGROW ins_test ins ins_full_ccusGrowT )
  target_compile_definitions(ins_full_ccusGrowT PRIVATE
    -D CMAP -D CPROB)
endif()
