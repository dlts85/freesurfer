project(mri_label2vol)

include_directories(${FS_INCLUDE_DIRS})

add_executable(mri_label2vol mri_label2vol.c)
target_link_libraries(mri_label2vol utils)

add_test_script(NAME mri_label2vol_test SCRIPT test.py DEPENDS mri_label2vol)

install(TARGETS mri_label2vol DESTINATION bin)
