' This file has been automatically generated by "kdu_hyperdoc"
' Do not edit manually.
' Copyright 2001, David Taubman, The University of New South Wales (UNSW)

Namespace kdu_mni
  Public Class Ckdu_lobal
    Inherits Ckdu_global_funcs
    Public Const KDU_INT16_MAX As Short = 32767
    Public Const KDU_INT16_MIN As Short = -32768
    Public Const KDU_INT32_MAX As Integer = 2147483647
    Public Const KDU_INT32_MIN As Integer = -2147483648
    Public Const LL_BAND As Integer = 0
    Public Const HL_BAND As Integer = 1
    Public Const LH_BAND As Integer = 2
    Public Const HH_BAND As Integer = 3
    Public Const KDU_FIX_POINT As Integer = 13
    Public Const KD_LINE_BUF_ABSOLUTE As Byte = 1
    Public Const KD_LINE_BUF_SHORTS As Byte = 2
    Public Const KDU_CORE_VERSION As String = "v6.1"
    Public Const KDU_SOC As Integer = 65359
    Public Const KDU_SOT As Integer = 65424
    Public Const KDU_SOD As Integer = 65427
    Public Const KDU_SOP As Integer = 65425
    Public Const KDU_EPH As Integer = 65426
    Public Const KDU_EOC As Integer = 65497
    Public Const KDU_SIZ As Integer = 65361
    Public Const KDU_CBD As Integer = 65400
    Public Const KDU_MCT As Integer = 65396
    Public Const KDU_MCC As Integer = 65397
    Public Const KDU_MCO As Integer = 65399
    Public Const KDU_COD As Integer = 65362
    Public Const KDU_COC As Integer = 65363
    Public Const KDU_QCD As Integer = 65372
    Public Const KDU_QCC As Integer = 65373
    Public Const KDU_RGN As Integer = 65374
    Public Const KDU_POC As Integer = 65375
    Public Const KDU_CRG As Integer = 65379
    Public Const KDU_DFS As Integer = 65394
    Public Const KDU_ADS As Integer = 65395
    Public Const KDU_ATK As Integer = 65401
    Public Const KDU_COM As Integer = 65380
    Public Const KDU_TLM As Integer = 65365
    Public Const KDU_PLM As Integer = 65367
    Public Const KDU_PLT As Integer = 65368
    Public Const KDU_PPM As Integer = 65376
    Public Const KDU_PPT As Integer = 65377
    Public Const KDU_WANT_OUTPUT_COMPONENTS As Integer = 0
    Public Const KDU_WANT_CODESTREAM_COMPONENTS As Integer = 1
    Public Const KDU_SOURCE_CAP_SEQUENTIAL As Integer = 1
    Public Const KDU_SOURCE_CAP_SEEKABLE As Integer = 2
    Public Const KDU_SOURCE_CAP_CACHED As Integer = 4
    Public Const KDU_SOURCE_CAP_IN_MEMORY As Integer = 8
    Public Const SIZ_params As String = "SIZ"
    Public Const Sprofile As String = "Sprofile"
    Public Const Scap As String = "Scap"
    Public Const Sextensions As String = "Sextensions"
    Public Const Ssize As String = "Ssize"
    Public Const Sorigin As String = "Sorigin"
    Public Const Stiles As String = "Stiles"
    Public Const Stile_origin As String = "Stile_origin"
    Public Const Scomponents As String = "Scomponents"
    Public Const Ssigned As String = "Ssigned"
    Public Const Sprecision As String = "Sprecision"
    Public Const Ssampling As String = "Ssampling"
    Public Const Sdims As String = "Sdims"
    Public Const Mcomponents As String = "Mcomponents"
    Public Const Msigned As String = "Msigned"
    Public Const Mprecision As String = "Mprecision"
    Public Const Sprofile_PROFILE0 As Integer = 0
    Public Const Sprofile_PROFILE1 As Integer = 1
    Public Const Sprofile_PROFILE2 As Integer = 2
    Public Const Sprofile_PART2 As Integer = 3
    Public Const Sprofile_CINEMA2K As Integer = 4
    Public Const Sprofile_CINEMA4K As Integer = 5
    Public Const Sextensions_DC As Integer = 1
    Public Const Sextensions_VARQ As Integer = 2
    Public Const Sextensions_TCQ As Integer = 4
    Public Const Sextensions_PRECQ As Integer = 2048
    Public Const Sextensions_VIS As Integer = 8
    Public Const Sextensions_SSO As Integer = 16
    Public Const Sextensions_DECOMP As Integer = 32
    Public Const Sextensions_ANY_KNL As Integer = 64
    Public Const Sextensions_SYM_KNL As Integer = 128
    Public Const Sextensions_MCT As Integer = 256
    Public Const Sextensions_CURVE As Integer = 512
    Public Const Sextensions_ROI As Integer = 1024
    Public Const MCT_params As String = "MCT"
    Public Const Mmatrix_size As String = "Mmatrix_size"
    Public Const Mmatrix_coeffs As String = "Mmatrix_coeffs"
    Public Const Mvector_size As String = "Mvector_size"
    Public Const Mvector_coeffs As String = "Mvector_coeffs"
    Public Const Mtriang_size As String = "Mtriang_size"
    Public Const Mtriang_coeffs As String = "Mtriang_coeffs"
    Public Const MCC_params As String = "MCC"
    Public Const Mstage_inputs As String = "Mstage_inputs"
    Public Const Mstage_outputs As String = "Mstage_outputs"
    Public Const Mstage_blocks As String = "Mstage_collections"
    Public Const Mstage_xforms As String = "Mstage_xforms"
    Public Const Mxform_DEP As Integer = 0
    Public Const Mxform_MATRIX As Integer = 9
    Public Const Mxform_DWT As Integer = 3
    Public Const Mxform_MAT As Integer = 1000
    Public Const MCO_params As String = "MCO"
    Public Const Mnum_stages As String = "Mnum_stages"
    Public Const Mstages As String = "Mstages"
    Public Const ATK_params As String = "ATK"
    Public Const Kreversible As String = "Kreversible"
    Public Const Ksymmetric As String = "Ksymmetric"
    Public Const Kextension As String = "Kextension"
    Public Const Ksteps As String = "Ksteps"
    Public Const Kcoeffs As String = "Kcoeffs"
    Public Const Kextension_CON As Integer = 0
    Public Const Kextension_SYM As Integer = 1
    Public Const COD_params As String = "COD"
    Public Const Cycc As String = "Cycc"
    Public Const Cmct As String = "Cmct"
    Public Const Clayers As String = "Clayers"
    Public Const Cuse_sop As String = "Cuse_sop"
    Public Const Cuse_eph As String = "Cuse_eph"
    Public Const Corder As String = "Corder"
    Public Const Calign_blk_last As String = "Calign_blk_last"
    Public Const Clevels As String = "Clevels"
    Public Const Cads As String = "Cads"
    Public Const Cdfs As String = "Cdfs"
    Public Const Cdecomp As String = "Cdecomp"
    Public Const Creversible As String = "Creversible"
    Public Const Ckernels As String = "Ckernels"
    Public Const Catk As String = "Catk"
    Public Const Cuse_precincts As String = "Cuse_precincts"
    Public Const Cprecincts As String = "Cprecincts"
    Public Const Cblk As String = "Cblk"
    Public Const Cmodes As String = "Cmodes"
    Public Const Cweight As String = "Cweight"
    Public Const Clev_weights As String = "Clev_weights"
    Public Const Cband_weights As String = "Cband_weights"
    Public Const Creslengths As String = "Creslengths"
    Public Const Cmct_ARRAY As Integer = 2
    Public Const Cmct_DWT As Integer = 4
    Public Const Corder_LRCP As Integer = 0
    Public Const Corder_RLCP As Integer = 1
    Public Const Corder_RPCL As Integer = 2
    Public Const Corder_PCRL As Integer = 3
    Public Const Corder_CPRL As Integer = 4
    Public Const Ckernels_W9X7 As Integer = 0
    Public Const Ckernels_W5X3 As Integer = 1
    Public Const Ckernels_ATK As Integer = -1
    Public Const Cmodes_BYPASS As Integer = 1
    Public Const Cmodes_RESET As Integer = 2
    Public Const Cmodes_RESTART As Integer = 4
    Public Const Cmodes_CAUSAL As Integer = 8
    Public Const Cmodes_ERTERM As Integer = 16
    Public Const Cmodes_SEGMARK As Integer = 32
    Public Const ADS_params As String = "ADS"
    Public Const Ddecomp As String = "Ddecomp"
    Public Const DOads As String = "DOads"
    Public Const DSads As String = "DSads"
    Public Const DFS_params As String = "DFS"
    Public Const DSdfs As String = "DSdfs"
    Public Const QCD_params As String = "QCD"
    Public Const Qguard As String = "Qguard"
    Public Const Qderived As String = "Qderived"
    Public Const Qabs_steps As String = "Qabs_steps"
    Public Const Qabs_ranges As String = "Qabs_ranges"
    Public Const Qstep As String = "Qstep"
    Public Const RGN_params As String = "RGN"
    Public Const Rshift As String = "Rshift"
    Public Const Rlevels As String = "Rlevels"
    Public Const Rweight As String = "Rweight"
    Public Const POC_params As String = "POC"
    Public Const Porder As String = "Porder"
    Public Const CRG_params As String = "CRG"
    Public Const CRGoffset As String = "CRGoffset"
    Public Const ORG_params As String = "ORG"
    Public Const ORGtparts As String = "ORGtparts"
    Public Const ORGgen_tlm As String = "ORGgen_tlm"
    Public Const ORGgen_plt As String = "ORGgen_plt"
    Public Const ORGtparts_R As Integer = 1
    Public Const ORGtparts_L As Integer = 2
    Public Const ORGtparts_C As Integer = 4
    Public Const KDU_ANALYSIS_LOW As Integer = 0
    Public Const KDU_ANALYSIS_HIGH As Integer = 1
    Public Const KDU_SYNTHESIS_LOW As Integer = 2
    Public Const KDU_SYNTHESIS_HIGH As Integer = 3
    Public Const KDU_TIFFTAG_Artist As Long = 20643842
    Public Const KDU_TIFFTAG_Copyright As Long = 2190999554
    Public Const KDU_TIFFTAG_HostComputer As Long = 20709378
    Public Const KDU_TIFFTAG_ImageDescription As Long = 17694722
    Public Const KDU_TIFFTAG_Make As Long = 17760258
    Public Const KDU_TIFFTAG_Model As Long = 17825794
    Public Const KDU_TIFFTAG_Software As Long = 19988482
    Public Const KDU_TIFFTAG_BitsPerSample As Long = 16908291
    Public Const KDU_TIFFTAG_ColorMap As Long = 20971523
    Public Const KDU_TIFFTAG_Compression As Long = 16973827
    Public Const KDU_TIFF_Compression_NONE As Integer = 1
    Public Const KDU_TIFF_Compression_CCITT As Integer = 2
    Public Const KDU_TIFF_Compression_PACKBITS As Integer = 32773
    Public Const KDU_TIFFTAG_ExtraSamples As Long = 22151171
    Public Const KDU_TIFF_ExtraSamples_UNDEFINED As Integer = 0
    Public Const KDU_TIFF_ExtraSamples_PREMULTIPLIED_ALPHA As Integer = 1
    Public Const KDU_TIFF_ExtraSamples_UNASSOCIATED_ALPHA As Integer = 1
    Public Const KDU_TIFFTAG_FillOrder As Long = 17432579
    Public Const KDU_TIFF_FillOrder_MSB_FIRST As Integer = 1
    Public Const KDU_TIFF_FillOrder_LSB_FIRST As Integer = 2
    Public Const KDU_TIFFTAG_GrayResponseCurve As Long = 19070979
    Public Const KDU_TIFFTAG_GrayResponseUnit As Long = 19005443
    Public Const KDU_TIFFTAG_ImageHeight16 As Long = 16842755
    Public Const KDU_TIFFTAG_ImageHeight32 As Long = 16842756
    Public Const KDU_TIFFTAG_ImageWidth16 As Long = 16777219
    Public Const KDU_TIFFTAG_ImageWidth32 As Long = 16777220
    Public Const KDU_TIFFTAG_InkSet As Long = 21757955
    Public Const KDU_TIFF_InkSet_CMYK As Integer = 1
    Public Const KDU_TIFF_InkSet_NotCMYK As Integer = 2
    Public Const KDU_TIFFTAG_MaxSampleValue As Long = 18415619
    Public Const KDU_TIFFTAG_MinSampleValue As Long = 18350083
    Public Const KDU_TIFFTAG_NumberOfInks As Long = 21889027
    Public Const KDU_TIFFTAG_PhotometricInterp As Long = 17170435
    Public Const KDU_TIFF_PhotometricInterp_WHITEISZERO As Integer = 0
    Public Const KDU_TIFF_PhotometricInterp_BLACKISZERO As Integer = 1
    Public Const KDU_TIFF_PhotometricInterp_RGB As Integer = 2
    Public Const KDU_TIFF_PhotometricInterp_PALETTE As Integer = 3
    Public Const KDU_TIFF_PhotometricInterp_TRANSPARENCY As Integer = 4
    Public Const KDU_TIFF_PhotometricInterp_SEPARATED As Integer = 5
    Public Const KDU_TIFF_PhotometricInterp_YCbCr As Integer = 6
    Public Const KDU_TIFF_PhotometricInterp_CIELab As Integer = 8
    Public Const KDU_TIFFTAG_PlanarConfig As Long = 18612227
    Public Const KDU_TIFF_PlanarConfig_CONTIG As Integer = 1
    Public Const KDU_TIFF_PlanarConfig_PLANAR As Integer = 2
    Public Const KDU_TIFFTAG_ResolutionUnit As Long = 19398659
    Public Const KDU_TIFF_ResolutionUnit_NONE As Integer = 1
    Public Const KDU_TIFF_ResolutionUnit_INCH As Integer = 2
    Public Const KDU_TIFF_ResolutionUnit_CM As Integer = 3
    Public Const KDU_TIFFTAG_RowsPerStrip16 As Long = 18219011
    Public Const KDU_TIFFTAG_RowsPerStrip32 As Long = 18219012
    Public Const KDU_TIFFTAG_SampleFormat As Long = 22216707
    Public Const KDU_TIFF_SampleFormat_UNSIGNED As Integer = 1
    Public Const KDU_TIFF_SampleFormat_SIGNED As Integer = 2
    Public Const KDU_TIFF_SampleFormat_FLOAT As Integer = 3
    Public Const KDU_TIFF_SampleFormat_UNDEFINED As Integer = 3
    Public Const KDU_TIFFTAG_SamplesPerPixel As Long = 18153475
    Public Const KDU_TIFFTAG_SminSampleValueF As Long = 22282251
    Public Const KDU_TIFFTAG_SminSampleValueD As Long = 22282252
    Public Const KDU_TIFFTAG_SmaxSampleValueF As Long = 22347787
    Public Const KDU_TIFFTAG_SmaxSampleValueD As Long = 22347788
    Public Const KDU_TIFFTAG_StripByteCounts16 As Long = 18284547
    Public Const KDU_TIFFTAG_StripByteCounts32 As Long = 18284548
    Public Const KDU_TIFFTAG_StripOffsets16 As Long = 17891331
    Public Const KDU_TIFFTAG_StripOffsets32 As Long = 17891332
    Public Const KDU_TIFFTAG_TileWidth16 As Long = 21102595
    Public Const KDU_TIFFTAG_TileWidth32 As Long = 21102596
    Public Const KDU_TIFFTAG_TileHeight16 As Long = 21168131
    Public Const KDU_TIFFTAG_TileHeight32 As Long = 21168132
    Public Const KDU_TIFFTAG_TileOffsets As Long = 21233668
    Public Const KDU_TIFFTAG_XResolution As Long = 18481157
    Public Const KDU_TIFFTAG_YResolution As Long = 18546693
    Public Const KDU_COMPOSITOR_SCALE_TOO_SMALL As Integer = 1
    Public Const KDU_COMPOSITOR_CANNOT_FLIP As Integer = 2
    Public Const KDU_SIMPLE_VIDEO_YCC As Long = 1
    Public Const KDU_SIMPLE_VIDEO_RGB As Long = 2
    Public Const jp2_signature_4cc As Long = 1783636000
    Public Const jp2_file_type_4cc As Long = 1718909296
    Public Const jp2_header_4cc As Long = 1785737832
    Public Const jp2_image_header_4cc As Long = 1768449138
    Public Const jp2_bits_per_component_4cc As Long = 1651532643
    Public Const jp2_colour_4cc As Long = 1668246642
    Public Const jp2_palette_4cc As Long = 1885564018
    Public Const jp2_component_mapping_4cc As Long = 1668112752
    Public Const jp2_channel_definition_4cc As Long = 1667523942
    Public Const jp2_resolution_4cc As Long = 1919251232
    Public Const jp2_capture_resolution_4cc As Long = 1919251299
    Public Const jp2_display_resolution_4cc As Long = 1919251300
    Public Const jp2_codestream_4cc As Long = 1785737827
    Public Const jp2_dtbl_4cc As Long = 1685348972
    Public Const jp2_data_entry_url_4cc As Long = 1970433056
    Public Const jp2_fragment_table_4cc As Long = 1718903404
    Public Const jp2_fragment_list_4cc As Long = 1718383476
    Public Const jp2_reader_requirements_4cc As Long = 1920099697
    Public Const jp2_codestream_header_4cc As Long = 1785750376
    Public Const jp2_desired_reproductions_4cc As Long = 1685218672
    Public Const jp2_compositing_layer_hdr_4cc As Long = 1785752680
    Public Const jp2_registration_4cc As Long = 1668441447
    Public Const jp2_opacity_4cc As Long = 1869636468
    Public Const jp2_colour_group_4cc As Long = 1667723888
    Public Const jp2_composition_4cc As Long = 1668246896
    Public Const jp2_comp_options_4cc As Long = 1668247668
    Public Const jp2_comp_instruction_set_4cc As Long = 1768846196
    Public Const jp2_iprights_4cc As Long = 1785737833
    Public Const jp2_uuid_4cc As Long = 1970628964
    Public Const jp2_uuid_info_4cc As Long = 1969843814
    Public Const jp2_label_4cc As Long = 1818389536
    Public Const jp2_xml_4cc As Long = 2020437024
    Public Const jp2_number_list_4cc As Long = 1852601204
    Public Const jp2_roi_description_4cc As Long = 1919904100
    Public Const jp2_association_4cc As Long = 1634955107
    Public Const mj2_movie_4cc As Long = 1836019574
    Public Const mj2_movie_header_4cc As Long = 1836476516
    Public Const mj2_track_4cc As Long = 1953653099
    Public Const mj2_track_header_4cc As Long = 1953196132
    Public Const mj2_media_4cc As Long = 1835297121
    Public Const mj2_media_header_4cc As Long = 1835296868
    Public Const mj2_media_header_typo_4cc As Long = 1835558002
    Public Const mj2_media_handler_4cc As Long = 1751411826
    Public Const mj2_media_information_4cc As Long = 1835626086
    Public Const mj2_video_media_header_4cc As Long = 1986881636
    Public Const mj2_video_handler_4cc As Long = 1986618469
    Public Const mj2_data_information_4cc As Long = 1684631142
    Public Const mj2_data_reference_4cc As Long = 1685218662
    Public Const mj2_url_4cc As Long = 1970433056
    Public Const mj2_sample_table_4cc As Long = 1937007212
    Public Const mj2_sample_description_4cc As Long = 1937011556
    Public Const mj2_visual_sample_entry_4cc As Long = 1835692082
    Public Const mj2_field_coding_4cc As Long = 1718183276
    Public Const mj2_sample_size_4cc As Long = 1937011578
    Public Const mj2_sample_to_chunk_4cc As Long = 1937011555
    Public Const mj2_chunk_offset_4cc As Long = 1937007471
    Public Const mj2_chunk_offset64_4cc As Long = 1668232756
    Public Const mj2_time_to_sample_4cc As Long = 1937011827
    Public Const jp2_mdat_4cc As Long = 1835295092
    Public Const jp2_free_4cc As Long = 1718773093
    Public Const mj2_skip_4cc As Long = 1936419184
    Public Const jp2_placeholder_4cc As Long = 1885891684
    Public Const JP2_COMPRESSION_TYPE_NONE As Integer = 0
    Public Const JP2_COMPRESSION_TYPE_MH As Integer = 1
    Public Const JP2_COMPRESSION_TYPE_MR As Integer = 2
    Public Const JP2_COMPRESSION_TYPE_MMR As Integer = 3
    Public Const JP2_COMPRESSION_TYPE_JBIG_B As Integer = 4
    Public Const JP2_COMPRESSION_TYPE_JPEG As Integer = 5
    Public Const JP2_COMPRESSION_TYPE_JLS As Integer = 6
    Public Const JP2_COMPRESSION_TYPE_JPEG2000 As Integer = 7
    Public Const JP2_COMPRESSION_TYPE_JBIG2 As Integer = 8
    Public Const JP2_COMPRESSION_TYPE_JBIG As Integer = 9
    Public Const JP2_bilevel1_SPACE  As Integer = 0
    Public Const JP2_YCbCr1_SPACE  As Integer = 1
    Public Const JP2_YCbCr2_SPACE  As Integer = 3
    Public Const JP2_YCbCr3_SPACE  As Integer = 4
    Public Const JP2_PhotoYCC_SPACE  As Integer = 9
    Public Const JP2_CMY_SPACE  As Integer = 11
    Public Const JP2_CMYK_SPACE  As Integer = 12
    Public Const JP2_YCCK_SPACE  As Integer = 13
    Public Const JP2_CIELab_SPACE  As Integer = 14
    Public Const JP2_bilevel2_SPACE  As Integer = 15
    Public Const JP2_sRGB_SPACE  As Integer = 16
    Public Const JP2_sLUM_SPACE  As Integer = 17
    Public Const JP2_sYCC_SPACE  As Integer = 18
    Public Const JP2_CIEJab_SPACE  As Integer = 19
    Public Const JP2_esRGB_SPACE  As Integer = 20
    Public Const JP2_ROMMRGB_SPACE  As Integer = 21
    Public Const JP2_YPbPr60_SPACE  As Integer = 22
    Public Const JP2_YPbPr50_SPACE  As Integer = 23
    Public Const JP2_esYCC_SPACE  As Integer = 24
    Public Const JP2_iccLUM_SPACE  As Integer = 100
    Public Const JP2_iccRGB_SPACE  As Integer = 101
    Public Const JP2_iccANY_SPACE  As Integer = 102
    Public Const JP2_vendor_SPACE  As Integer = 200
    Public Const JP2_CIE_D50 As Long = 4470064
    Public Const JP2_CIE_D65 As Long = 4470325
    Public Const JP2_CIE_D75 As Long = 4470581
    Public Const JP2_CIE_SA As Long = 21313
    Public Const JP2_CIE_SC As Long = 21315
    Public Const JP2_CIE_F2 As Long = 17970
    Public Const JP2_CIE_F7 As Long = 17975
    Public Const JP2_CIE_F11 As Long = 4600113
    Public Const JPX_SF_CODESTREAM_NO_EXTENSIONS As Integer = 1
    Public Const JPX_SF_MULTIPLE_LAYERS As Integer = 2
    Public Const JPX_SF_JPEG2000_PART1_PROFILE0 As Integer = 3
    Public Const JPX_SF_JPEG2000_PART1_PROFILE1 As Integer = 4
    Public Const JPX_SF_JPEG2000_PART1 As Integer = 5
    Public Const JPX_SF_JPEG2000_PART2 As Integer = 6
    Public Const JPX_SF_CODESTREAM_USING_DCT As Integer = 7
    Public Const JPX_SF_NO_OPACITY As Integer = 8
    Public Const JPX_SF_OPACITY_NOT_PREMULTIPLIED As Integer = 9
    Public Const JPX_SF_OPACITY_PREMULTIPLIED As Integer = 10
    Public Const JPX_SF_OPACITY_BY_CHROMA_KEY As Integer = 11
    Public Const JPX_SF_CODESTREAM_CONTIGUOUS As Integer = 12
    Public Const JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL_AND_ORDERED As Integer = 13
    Public Const JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL As Integer = 14
    Public Const JPX_SF_CODESTREAM_FRAGMENTS_LOCAL As Integer = 15
    Public Const JPX_SF_CODESTREAM_FRAGMENTS_REMOTE As Integer = 16
    Public Const JPX_SF_COMPOSITING_USED As Integer = 17
    Public Const JPX_SF_COMPOSITING_NOT_REQUIRED As Integer = 18
    Public Const JPX_SF_MULTIPLE_LAYERS_NO_COMPOSITING_OR_ANIMATION As Integer = 19
    Public Const JPX_SF_ONE_CODESTREAM_PER_LAYER As Integer = 20
    Public Const JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER As Integer = 21
    Public Const JPX_SF_SINGLE_COLOUR_SPACE As Integer = 22
    Public Const JPX_SF_MULTIPLE_COLOUR_SPACES As Integer = 23
    Public Const JPX_SF_NO_ANIMATION As Integer = 24
    Public Const JPX_SF_ANIMATED_COVERED_BY_FIRST_LAYER As Integer = 25
    Public Const JPX_SF_ANIMATED_NOT_COVERED_BY_FIRST_LAYER As Integer = 26
    Public Const JPX_SF_ANIMATED_LAYERS_NOT_REUSED As Integer = 27
    Public Const JPX_SF_ANIMATED_LAYERS_REUSED As Integer = 28
    Public Const JPX_SF_ANIMATED_PERSISTENT_FRAMES As Integer = 29
    Public Const JPX_SF_ANIMATED_NON_PERSISTENT_FRAMES As Integer = 30
    Public Const JPX_SF_NO_SCALING As Integer = 31
    Public Const JPX_SF_SCALING_WITHIN_LAYER As Integer = 32
    Public Const JPX_SF_SCALING_BETWEEN_LAYERS As Integer = 33
    Public Const JPX_SF_ROI_METADATA As Integer = 34
    Public Const JPX_SF_IPR_METADATA As Integer = 35
    Public Const JPX_SF_CONTENT_METADATA As Integer = 36
    Public Const JPX_SF_HISTORY_METADATA As Integer = 37
    Public Const JPX_SF_CREATION_METADATA As Integer = 38
    Public Const JPX_SF_DIGITALLY_SIGNED As Integer = 39
    Public Const JPX_SF_CHECKSUMMED As Integer = 40
    Public Const JPX_SF_DESIRED_REPRODUCTION As Integer = 41
    Public Const JPX_SF_PALETTIZED_COLOUR As Integer = 42
    Public Const JPX_SF_RESTRICTED_ICC As Integer = 43
    Public Const JPX_SF_ANY_ICC As Integer = 44
    Public Const JPX_SF_sRGB As Integer = 45
    Public Const JPX_SF_sLUM As Integer = 46
    Public Const JPX_SF_sYCC As Integer = 70
    Public Const JPX_SF_BILEVEL1 As Integer = 47
    Public Const JPX_SF_BILEVEL2 As Integer = 48
    Public Const JPX_SF_YCbCr1 As Integer = 49
    Public Const JPX_SF_YCbCr2 As Integer = 50
    Public Const JPX_SF_YCbCr3 As Integer = 51
    Public Const JPX_SF_PhotoYCC As Integer = 52
    Public Const JPX_SF_YCCK As Integer = 53
    Public Const JPX_SF_CMY As Integer = 54
    Public Const JPX_SF_CMYK As Integer = 55
    Public Const JPX_SF_LAB_DEFAULT As Integer = 56
    Public Const JPX_SF_LAB As Integer = 57
    Public Const JPX_SF_JAB_DEFAULT As Integer = 58
    Public Const JPX_SF_JAB As Integer = 59
    Public Const JPX_SF_esRGB As Integer = 60
    Public Const JPX_SF_ROMMRGB As Integer = 61
    Public Const JPX_SF_SAMPLES_NOT_SQUARE As Integer = 62
    Public Const JPX_SF_LAYERS_HAVE_LABELS As Integer = 63
    Public Const JPX_SF_CODESTREAMS_HAVE_LABELS As Integer = 64
    Public Const JPX_SF_MULTIPLE_COLOUR_SPACES2 As Integer = 65
    Public Const JPX_SF_LAYERS_HAVE_DIFFERENT_METADATA As Integer = 66
    Public Const MJ2_GRAPHICS_COPY As Short = 0
    Public Const MJ2_GRAPHICS_BLUE_SCREEN As Short = 36
    Public Const MJ2_GRAPHICS_ALPHA As Short = 256
    Public Const MJ2_GRAPHICS_PREMULT_ALPHA As Short = 257
    Public Const MJ2_GRAPHICS_COMPONENT_ALPHA As Short = 272
    Public Const MJ2_TRACK_NON_EXISTENT As Integer = 0
    Public Const MJ2_TRACK_MAY_EXIST As Integer = -1
    Public Const MJ2_TRACK_IS_VIDEO As Integer = 1
    Public Const MJ2_TRACK_IS_OTHER As Integer = 1000
    Public Const KDU_JPIP_CONTEXT_NONE As Integer = 0
    Public Const KDU_JPIP_CONTEXT_JPXL As Integer = 1
    Public Const KDU_JPIP_CONTEXT_MJ2T As Integer = 2
    Public Const KDU_JPIP_CONTEXT_TRANSLATED As Integer = -1
    Public Const KDU_MRQ_ALL As Integer = 1
    Public Const KDU_MRQ_GLOBAL As Integer = 2
    Public Const KDU_MRQ_STREAM As Integer = 4
    Public Const KDU_MRQ_WINDOW As Integer = 8
    Public Const KDU_MRQ_DEFAULT As Integer = 14
    Public Const KDU_MRQ_ANY As Integer = 15
    Public Const KDS_METASCOPE_MANDATORY As Integer = 1
    Public Const KDS_METASCOPE_IMAGE_MANDATORY As Integer = 2
    Public Const KDS_METASCOPE_INCLUDE_NEXT_SIBLING As Integer = 16
    Public Const KDS_METASCOPE_LEAF As Integer = 32
    Public Const KDS_METASCOPE_HAS_GLOBAL_DATA As Integer = 256
    Public Const KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA As Integer = 512
    Public Const KDS_METASCOPE_HAS_IMAGE_WIDE_DATA As Integer = 4096
    Public Const KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA As Integer = 8192
  End Class
End Namespace
