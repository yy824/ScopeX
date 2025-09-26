install(
    TARGETS scopeX_exe
    RUNTIME COMPONENT scopeX_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
