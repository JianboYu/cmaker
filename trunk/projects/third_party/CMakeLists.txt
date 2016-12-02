AUX_SOURCE_DIRECTORY(./aac/libAACdec/src aacdec_src)
AUX_SOURCE_DIRECTORY(./aac/libAACenc/src aacenc_src)
AUX_SOURCE_DIRECTORY(./aac/libPCMutils/src pcmuitls_src)
AUX_SOURCE_DIRECTORY(./aac/libFDK/src fdk_src)
AUX_SOURCE_DIRECTORY(./aac/libSYS/src sys_src)
AUX_SOURCE_DIRECTORY(./aac/libMpegTPDec/src mpegtpdec_src)
AUX_SOURCE_DIRECTORY(./aac/libMpegTPEnc/src mpegtpenc_src)
AUX_SOURCE_DIRECTORY(./aac/libSBRdec/src sbrdec_src)
AUX_SOURCE_DIRECTORY(./aac/libSBRenc/src sbrenc_src)

INCLUDE_DIRECTORIES("./aac/libAACdec/include"
                    "./aac/libAACenc/include"
                    "./aac/libPCMutils/include"
                    "./aac/libFDK/include"
                    "./aac/libSYS/include"
                    "./aac/libMpegTPDec/include"
                    "./aac/libMpegTPEnc/include"
                    "./aac/libSBRdec/include"
                    "./aac/libSBRenc/include"
                    )

ADD_LIBRARY(libFraunhoferAAC STATIC ${aacdec_src}
            ${aacenc_src}
            ${pcmuitls_src}
            ${fdk_src}
            ${sys_src}
            ${mpegtpdec_src}
            ${mpegtpenc_src}
            ${sbrdec_src}
            ${sbrenc_src}
           ) 
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_PRJ_CXX_FLAGS} --std=c++98 -Wno-error")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_PRJ_C_FLAGS} -Wno-sequence-point -Wno-extra -Wno-error")
MESSAGE(INFO " libFraunhoferAAC CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
MESSAGE(INFO " libFraunhoferAAC CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
SET_TARGET_PROPERTIES(libFraunhoferAAC PROPERTIES OUTPUT_NAME "FraunhoferAAC")

ENABLE_LANGUAGE(C ASM)
SET(avce_src_c
    "./libavc/common/ih264_resi_trans_quant.c"
    "./libavc/common/ih264_iquant_itrans_recon.c"
    "./libavc/common/ih264_ihadamard_scaling.c"
    "./libavc/common/ih264_inter_pred_filters.c"
    "./libavc/common/ih264_luma_intra_pred_filters.c"
    "./libavc/common/ih264_chroma_intra_pred_filters.c"
    "./libavc/common/ih264_padding.c"
    "./libavc/common/ih264_mem_fns.c"
    "./libavc/common/ih264_deblk_edge_filters.c"
    "./libavc/common/ih264_deblk_tables.c"
    "./libavc/common/ih264_cavlc_tables.c"
    "./libavc/common/ih264_cabac_tables.c"
    "./libavc/common/ih264_common_tables.c"
    "./libavc/common/ih264_trans_data.c"
    "./libavc/common/ih264_buf_mgr.c"
    "./libavc/common/ih264_dpb_mgr.c"
    "./libavc/common/ih264_list.c"
    "./libavc/common/ithread.c"
    "./libavc/encoder/ih264e_globals.c"
    "./libavc/encoder/ih264e_intra_modes_eval.c"
    "./libavc/encoder/ih264e_half_pel.c"
    "./libavc/encoder/ih264e_mc.c"
    "./libavc/encoder/ih264e_me.c"
    "./libavc/encoder/ih264e_rc_mem_interface.c"
    "./libavc/encoder/ih264e_time_stamp.c"
    "./libavc/encoder/ih264e_modify_frm_rate.c"
    "./libavc/encoder/ih264e_rate_control.c"
    "./libavc/encoder/ih264e_core_coding.c"
    "./libavc/encoder/ih264e_deblk.c"
    "./libavc/encoder/ih264e_api.c"
    "./libavc/encoder/ih264e_process.c"
    "./libavc/encoder/ih264e_encode.c"
    "./libavc/encoder/ih264e_utils.c"
    "./libavc/encoder/ih264e_version.c"
    "./libavc/encoder/ih264e_bitstream.c"
    "./libavc/encoder/ih264e_cavlc.c"
    "./libavc/encoder/ih264e_cabac_init.c"
    "./libavc/encoder/ih264e_cabac.c"
    "./libavc/encoder/ih264e_cabac_encode.c"
    "./libavc/encoder/ih264e_encode_header.c"
    "./libavc/encoder/ih264e_function_selector_generic.c"
    "./libavc/encoder/ih264e_fmt_conv.c"
    "./libavc/encoder/irc_rate_control_api.c"
    "./libavc/encoder/irc_bit_allocation.c"
    "./libavc/encoder/irc_cbr_buffer_control.c"
    "./libavc/encoder/irc_est_sad.c"
    "./libavc/encoder/irc_fixed_point_error_bits.c"
    "./libavc/encoder/irc_frame_info_collector.c"
    "./libavc/encoder/irc_mb_model_based.c"
    "./libavc/encoder/irc_picture_type.c"
    "./libavc/encoder/irc_rd_model.c"
    "./libavc/encoder/irc_vbr_storage_vbv.c"
    "./libavc/encoder/irc_vbr_str_prms.c"
    "./libavc/encoder/ime.c"
    "./libavc/encoder/ime_distortion_metrics.c")

if (ARCH_x86_64)
  SET(ARCH_CFLAGS "-DX86 -msse4.2")
  SET(ARCH "x86")
  
  SET(avce_src_arch_c
      "./libavc/encoder/x86/ih264e_function_selector.c"
      "./libavc/encoder/x86/ih264e_function_selector_sse42.c"
      "./libavc/encoder/x86/ih264e_function_selector_ssse3.c"
      "./libavc/encoder/x86/ih264e_half_pel_ssse3.c"
      "./libavc/encoder/x86/ih264e_intra_modes_eval_ssse3.c"
      "./libavc/encoder/x86/ime_distortion_metrics_sse42.c"
      "./libavc/common/x86/ih264_iquant_itrans_recon_ssse3.c"
      "./libavc/common/x86/ih264_iquant_itrans_recon_dc_ssse3.c"
      "./libavc/common/x86/ih264_ihadamard_scaling_ssse3.c"
      "./libavc/common/x86/ih264_inter_pred_filters_ssse3.c"
      "./libavc/common/x86/ih264_mem_fns_ssse3.c"
      "./libavc/common/x86/ih264_padding_ssse3.c"
      "./libavc/common/x86/ih264_luma_intra_pred_filters_ssse3.c"
      "./libavc/common/x86/ih264_chroma_intra_pred_filters_ssse3.c"
      "./libavc/common/x86/ih264_deblk_chroma_ssse3.c"
      "./libavc/common/x86/ih264_deblk_luma_ssse3.c"
      "./libavc/common/x86/ih264_iquant_itrans_recon_sse42.c"
      "./libavc/common/x86/ih264_ihadamard_scaling_sse42.c"
      "./libavc/common/x86/ih264_resi_trans_quant_sse42.c"
      "./libavc/common/x86/ih264_weighted_pred_sse42.c")
elseif(ARCH_arm)
  SET(ARCH_CFLAGS "-DARM")
  SET(ARCH "arm")
  #ENABLE_LANGUAGE(C ASM)
 
  SET(avce_src_arch_c
      "./libavc/encoder/arm/ih264e_function_selector.c")

  if(ARCH_ARM_HAVE_NEON)
    SET(avce_src_arch "${avce_src_arch}"
      "./libavc/encoder/arm/ih264e_function_selector_a9q.c"
      "./libavc/common/arm/ih264_resi_trans_quant_a9.s"
      "./libavc/common/arm/ih264_iquant_itrans_recon_a9.s"
      "./libavc/common/arm/ih264_iquant_itrans_recon_dc_a9.s"
      "./libavc/common/arm/ih264_ihadamard_scaling_a9.s"
      "./libavc/common/arm/ih264_deblk_chroma_a9.s"
      "./libavc/common/arm/ih264_deblk_luma_a9.s"
      "./libavc/common/arm/ih264_intra_pred_chroma_a9q.s"
      "./libavc/common/arm/ih264_intra_pred_luma_16x16_a9q.s"
      "./libavc/common/arm/ih264_intra_pred_luma_4x4_a9q.s"
      "./libavc/common/arm/ih264_intra_pred_luma_8x8_a9q.s"
      "./libavc/common/arm/ih264_inter_pred_chroma_a9q.s"
      "./libavc/common/arm/ih264_inter_pred_filters_luma_horz_a9q.s"
      "./libavc/common/arm/ih264_inter_pred_filters_luma_vert_a9q.s"
      "./libavc/common/arm/ih264_inter_pred_luma_bilinear_a9q.s"
      "./libavc/common/arm/ih264_inter_pred_luma_copy_a9q.s"
      "./libavc/common/arm/ih264_padding_neon.s"
      "./libavc/common/arm/ih264_mem_fns_neon.s"
      "./libavc/encoder/arm/ih264e_evaluate_intra16x16_modes_a9q.s"
      "./libavc/encoder/arm/ih264e_evaluate_intra4x4_modes_a9q.s"
      "./libavc/encoder/arm/ih264e_evaluate_intra_chroma_modes_a9q.s"
      "./libavc/encoder/arm/ih264e_half_pel.s"
      "./libavc/encoder/arm/ih264e_fmt_conv.s"
      "./libavc/encoder/arm/ime_distortion_metrics_a9q.s")

  else(ARCH_ARM_HAVE_NEON) #No Neon
    SET(ARCH_CFLAGS "${ARCH_CFLAGS} -DDISABLE_NEON")
  endif() #Neon check

  SET(avce_src_arch_asm "./libavc/common/arm/ih264_arm_memory_barrier.s")

else(ARCH_ARM64)
endif ()

INCLUDE_DIRECTORIES(./libavc/encoder ./libavc/common ./libavc/encoder/${ARCH}/ ./libavc/common/${ARCH}/)
MESSAGE(INFO " libavcenc ARCH_INCLUDES: ${ARCH_INCLUDES}")
MESSAGE(INFO " libavcenc ASM_COMPILER: ${CMAKE_ASM_COMPILER}")
MESSAGE(INFO " libavcenc ASM_EXTENSIONS: ${CMAKE_ASM_SOURCE_FILE_EXTENSIONS}")

ADD_LIBRARY(libavcenc STATIC ${avce_src_c} ${avce_src_arch_c} ${avce_src_arch_asm})

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_PRJ_C_FLAGS} ${ARCH_CFLAGS} -DNDEBUG -UHP_PL -DN_MB_ENABLE -fPIC -O3")
MESSAGE(INFO " libavcenc CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
MESSAGE(INFO " libavcenc CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
SET_TARGET_PROPERTIES(libavcenc PROPERTIES OUTPUT_NAME "avcenc")

#AUX_SOURCE_DIRECTORY(./libavc/test/encoder/ avce_test_src)
#MESSAGE(INFO " AVCE_TEST_SRC: ${avce_test_src}")
#INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/projects/third_party/libavc/encoder)
#INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/projects/third_party/libavc/common)
#INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/projects/third_party/libavc/test/encoder)
#
#if (ANDROID)
#  SET(CMAKE_EXE_LINKER_FLAGS "-pie -fPIE -llog ${CMAKE_EXE_LINKER_FLAGS}")
#endif (ANDROID)
#
#SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
#MESSAGE(INFO " CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
#
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_PRJ_CXX_FLAGS}")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_PRJ_C_FLAGS} -DPROFILE_ENABLE -DARM -DMD5_DISABLE -fPIC -Wno-error")
#MESSAGE(INFO " avce_utest CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
#MESSAGE(INFO " avce_utest CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
#
#ADD_EXECUTABLE(avce_utest ${avce_test_src})
#SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
#TARGET_LINK_LIBRARIES(avce_utest libavcenc)
#TARGET_LINK_LIBRARIES(avce_utest pthread m)
