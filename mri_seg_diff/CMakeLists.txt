project(mri_seg_diff)

include_directories(${FS_INCLUDE_DIRS})

add_executable(mri_seg_diff mri_seg_diff.c)
target_link_libraries(mri_seg_diff utils)

add_test_script(NAME mri_seg_diff_test SCRIPT test.py DEPENDS mri_seg_diff)

install(TARGETS mri_seg_diff DESTINATION bin)
