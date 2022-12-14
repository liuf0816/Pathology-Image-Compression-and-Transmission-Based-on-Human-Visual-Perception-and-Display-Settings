// This file has been automatically generated by "kdu_hyperdoc"
// Do not edit manually.
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)

namespace kdu_mni {
  public class Ckdu_global : Ckdu_global_funcs {
    public const short KDU_INT16_MAX = unchecked((short) 0x7FFF);
    public const short KDU_INT16_MIN = unchecked((short) 0x8000);
    public const int KDU_INT32_MAX = unchecked((int) 0x7FFFFFFF);
    public const int KDU_INT32_MIN = unchecked((int) 0x80000000);
    public const int LL_BAND = unchecked((int) 0);
    public const int HL_BAND = unchecked((int) 1);
    public const int LH_BAND = unchecked((int) 2);
    public const int HH_BAND = unchecked((int) 3);
    public const int KDU_FIX_POINT = unchecked((int) 13);
    public const byte KD_LINE_BUF_ABSOLUTE = unchecked((byte) 1);
    public const byte KD_LINE_BUF_SHORTS = unchecked((byte) 2);
    public const string KDU_CORE_VERSION = unchecked((string) "v6.1");
    public const int KDU_SOC = unchecked((int) 0xFF4F);
    public const int KDU_SOT = unchecked((int) 0xFF90);
    public const int KDU_SOD = unchecked((int) 0xFF93);
    public const int KDU_SOP = unchecked((int) 0xFF91);
    public const int KDU_EPH = unchecked((int) 0xFF92);
    public const int KDU_EOC = unchecked((int) 0xFFD9);
    public const int KDU_SIZ = unchecked((int) 0xFF51);
    public const int KDU_CBD = unchecked((int) 0xFF78);
    public const int KDU_MCT = unchecked((int) 0xFF74);
    public const int KDU_MCC = unchecked((int) 0xFF75);
    public const int KDU_MCO = unchecked((int) 0xFF77);
    public const int KDU_COD = unchecked((int) 0xFF52);
    public const int KDU_COC = unchecked((int) 0xFF53);
    public const int KDU_QCD = unchecked((int) 0xFF5C);
    public const int KDU_QCC = unchecked((int) 0xFF5D);
    public const int KDU_RGN = unchecked((int) 0xFF5E);
    public const int KDU_POC = unchecked((int) 0xFF5F);
    public const int KDU_CRG = unchecked((int) 0xFF63);
    public const int KDU_DFS = unchecked((int) 0xFF72);
    public const int KDU_ADS = unchecked((int) 0xFF73);
    public const int KDU_ATK = unchecked((int) 0xFF79);
    public const int KDU_COM = unchecked((int) 0xFF64);
    public const int KDU_TLM = unchecked((int) 0xFF55);
    public const int KDU_PLM = unchecked((int) 0xFF57);
    public const int KDU_PLT = unchecked((int) 0xFF58);
    public const int KDU_PPM = unchecked((int) 0xFF60);
    public const int KDU_PPT = unchecked((int) 0xFF61);
    public const int KDU_WANT_OUTPUT_COMPONENTS = unchecked((int) 0);
    public const int KDU_WANT_CODESTREAM_COMPONENTS = unchecked((int) 1);
    public const int KDU_SOURCE_CAP_SEQUENTIAL = unchecked((int) 0x0001);
    public const int KDU_SOURCE_CAP_SEEKABLE = unchecked((int) 0x0002);
    public const int KDU_SOURCE_CAP_CACHED = unchecked((int) 0x0004);
    public const int KDU_SOURCE_CAP_IN_MEMORY = unchecked((int) 0x0008);
    public const string SIZ_params = unchecked((string) "SIZ");
    public const string Sprofile = unchecked((string) "Sprofile");
    public const string Scap = unchecked((string) "Scap");
    public const string Sextensions = unchecked((string) "Sextensions");
    public const string Ssize = unchecked((string) "Ssize");
    public const string Sorigin = unchecked((string) "Sorigin");
    public const string Stiles = unchecked((string) "Stiles");
    public const string Stile_origin = unchecked((string) "Stile_origin");
    public const string Scomponents = unchecked((string) "Scomponents");
    public const string Ssigned = unchecked((string) "Ssigned");
    public const string Sprecision = unchecked((string) "Sprecision");
    public const string Ssampling = unchecked((string) "Ssampling");
    public const string Sdims = unchecked((string) "Sdims");
    public const string Mcomponents = unchecked((string) "Mcomponents");
    public const string Msigned = unchecked((string) "Msigned");
    public const string Mprecision = unchecked((string) "Mprecision");
    public const int Sprofile_PROFILE0 = unchecked((int) 0);
    public const int Sprofile_PROFILE1 = unchecked((int) 1);
    public const int Sprofile_PROFILE2 = unchecked((int) 2);
    public const int Sprofile_PART2 = unchecked((int) 3);
    public const int Sprofile_CINEMA2K = unchecked((int) 4);
    public const int Sprofile_CINEMA4K = unchecked((int) 5);
    public const int Sextensions_DC = unchecked((int) 1);
    public const int Sextensions_VARQ = unchecked((int) 2);
    public const int Sextensions_TCQ = unchecked((int) 4);
    public const int Sextensions_PRECQ = unchecked((int) 2048);
    public const int Sextensions_VIS = unchecked((int) 8);
    public const int Sextensions_SSO = unchecked((int) 16);
    public const int Sextensions_DECOMP = unchecked((int) 32);
    public const int Sextensions_ANY_KNL = unchecked((int) 64);
    public const int Sextensions_SYM_KNL = unchecked((int) 128);
    public const int Sextensions_MCT = unchecked((int) 256);
    public const int Sextensions_CURVE = unchecked((int) 512);
    public const int Sextensions_ROI = unchecked((int) 1024);
    public const string MCT_params = unchecked((string) "MCT");
    public const string Mmatrix_size = unchecked((string) "Mmatrix_size");
    public const string Mmatrix_coeffs = unchecked((string) "Mmatrix_coeffs");
    public const string Mvector_size = unchecked((string) "Mvector_size");
    public const string Mvector_coeffs = unchecked((string) "Mvector_coeffs");
    public const string Mtriang_size = unchecked((string) "Mtriang_size");
    public const string Mtriang_coeffs = unchecked((string) "Mtriang_coeffs");
    public const string MCC_params = unchecked((string) "MCC");
    public const string Mstage_inputs = unchecked((string) "Mstage_inputs");
    public const string Mstage_outputs = unchecked((string) "Mstage_outputs");
    public const string Mstage_blocks = unchecked((string) "Mstage_collections");
    public const string Mstage_xforms = unchecked((string) "Mstage_xforms");
    public const int Mxform_DEP = unchecked((int) 0);
    public const int Mxform_MATRIX = unchecked((int) 9);
    public const int Mxform_DWT = unchecked((int) 3);
    public const int Mxform_MAT = unchecked((int) 1000);
    public const string MCO_params = unchecked((string) "MCO");
    public const string Mnum_stages = unchecked((string) "Mnum_stages");
    public const string Mstages = unchecked((string) "Mstages");
    public const string ATK_params = unchecked((string) "ATK");
    public const string Kreversible = unchecked((string) "Kreversible");
    public const string Ksymmetric = unchecked((string) "Ksymmetric");
    public const string Kextension = unchecked((string) "Kextension");
    public const string Ksteps = unchecked((string) "Ksteps");
    public const string Kcoeffs = unchecked((string) "Kcoeffs");
    public const int Kextension_CON = unchecked((int) 0);
    public const int Kextension_SYM = unchecked((int) 1);
    public const string COD_params = unchecked((string) "COD");
    public const string Cycc = unchecked((string) "Cycc");
    public const string Cmct = unchecked((string) "Cmct");
    public const string Clayers = unchecked((string) "Clayers");
    public const string Cuse_sop = unchecked((string) "Cuse_sop");
    public const string Cuse_eph = unchecked((string) "Cuse_eph");
    public const string Corder = unchecked((string) "Corder");
    public const string Calign_blk_last = unchecked((string) "Calign_blk_last");
    public const string Clevels = unchecked((string) "Clevels");
    public const string Cads = unchecked((string) "Cads");
    public const string Cdfs = unchecked((string) "Cdfs");
    public const string Cdecomp = unchecked((string) "Cdecomp");
    public const string Creversible = unchecked((string) "Creversible");
    public const string Ckernels = unchecked((string) "Ckernels");
    public const string Catk = unchecked((string) "Catk");
    public const string Cuse_precincts = unchecked((string) "Cuse_precincts");
    public const string Cprecincts = unchecked((string) "Cprecincts");
    public const string Cblk = unchecked((string) "Cblk");
    public const string Cmodes = unchecked((string) "Cmodes");
    public const string Cweight = unchecked((string) "Cweight");
    public const string Clev_weights = unchecked((string) "Clev_weights");
    public const string Cband_weights = unchecked((string) "Cband_weights");
    public const string Creslengths = unchecked((string) "Creslengths");
    public const int Cmct_ARRAY = unchecked((int) 2);
    public const int Cmct_DWT = unchecked((int) 4);
    public const int Corder_LRCP = unchecked((int) 0);
    public const int Corder_RLCP = unchecked((int) 1);
    public const int Corder_RPCL = unchecked((int) 2);
    public const int Corder_PCRL = unchecked((int) 3);
    public const int Corder_CPRL = unchecked((int) 4);
    public const int Ckernels_W9X7 = unchecked((int) 0);
    public const int Ckernels_W5X3 = unchecked((int) 1);
    public const int Ckernels_ATK = unchecked((int) -1);
    public const int Cmodes_BYPASS = unchecked((int) 1);
    public const int Cmodes_RESET = unchecked((int) 2);
    public const int Cmodes_RESTART = unchecked((int) 4);
    public const int Cmodes_CAUSAL = unchecked((int) 8);
    public const int Cmodes_ERTERM = unchecked((int) 16);
    public const int Cmodes_SEGMARK = unchecked((int) 32);
    public const string ADS_params = unchecked((string) "ADS");
    public const string Ddecomp = unchecked((string) "Ddecomp");
    public const string DOads = unchecked((string) "DOads");
    public const string DSads = unchecked((string) "DSads");
    public const string DFS_params = unchecked((string) "DFS");
    public const string DSdfs = unchecked((string) "DSdfs");
    public const string QCD_params = unchecked((string) "QCD");
    public const string Qguard = unchecked((string) "Qguard");
    public const string Qderived = unchecked((string) "Qderived");
    public const string Qabs_steps = unchecked((string) "Qabs_steps");
    public const string Qabs_ranges = unchecked((string) "Qabs_ranges");
    public const string Qstep = unchecked((string) "Qstep");
    public const string RGN_params = unchecked((string) "RGN");
    public const string Rshift = unchecked((string) "Rshift");
    public const string Rlevels = unchecked((string) "Rlevels");
    public const string Rweight = unchecked((string) "Rweight");
    public const string POC_params = unchecked((string) "POC");
    public const string Porder = unchecked((string) "Porder");
    public const string CRG_params = unchecked((string) "CRG");
    public const string CRGoffset = unchecked((string) "CRGoffset");
    public const string ORG_params = unchecked((string) "ORG");
    public const string ORGtparts = unchecked((string) "ORGtparts");
    public const string ORGgen_tlm = unchecked((string) "ORGgen_tlm");
    public const string ORGgen_plt = unchecked((string) "ORGgen_plt");
    public const int ORGtparts_R = unchecked((int) 1);
    public const int ORGtparts_L = unchecked((int) 2);
    public const int ORGtparts_C = unchecked((int) 4);
    public const int KDU_ANALYSIS_LOW = unchecked((int) 0);
    public const int KDU_ANALYSIS_HIGH = unchecked((int) 1);
    public const int KDU_SYNTHESIS_LOW = unchecked((int) 2);
    public const int KDU_SYNTHESIS_HIGH = unchecked((int) 3);
    public const long KDU_TIFFTAG_Artist = unchecked((long) 0x013B0002);
    public const long KDU_TIFFTAG_Copyright = unchecked((long) 0x82980002);
    public const long KDU_TIFFTAG_HostComputer = unchecked((long) 0x013C0002);
    public const long KDU_TIFFTAG_ImageDescription = unchecked((long) 0x010E0002);
    public const long KDU_TIFFTAG_Make = unchecked((long) 0x010F0002);
    public const long KDU_TIFFTAG_Model = unchecked((long) 0x01100002);
    public const long KDU_TIFFTAG_Software = unchecked((long) 0x01310002);
    public const long KDU_TIFFTAG_BitsPerSample = unchecked((long) 0x01020003);
    public const long KDU_TIFFTAG_ColorMap = unchecked((long) 0x01400003);
    public const long KDU_TIFFTAG_Compression = unchecked((long) 0x01030003);
    public const int KDU_TIFF_Compression_NONE = unchecked((int) 1);
    public const int KDU_TIFF_Compression_CCITT = unchecked((int) 2);
    public const int KDU_TIFF_Compression_PACKBITS = unchecked((int) 32773);
    public const long KDU_TIFFTAG_ExtraSamples = unchecked((long) 0x01520003);
    public const int KDU_TIFF_ExtraSamples_UNDEFINED = unchecked((int) 0);
    public const int KDU_TIFF_ExtraSamples_PREMULTIPLIED_ALPHA = unchecked((int) 1);
    public const int KDU_TIFF_ExtraSamples_UNASSOCIATED_ALPHA = unchecked((int) 1);
    public const long KDU_TIFFTAG_FillOrder = unchecked((long) 0x010A0003);
    public const int KDU_TIFF_FillOrder_MSB_FIRST = unchecked((int) 1);
    public const int KDU_TIFF_FillOrder_LSB_FIRST = unchecked((int) 2);
    public const long KDU_TIFFTAG_GrayResponseCurve = unchecked((long) 0x01230003);
    public const long KDU_TIFFTAG_GrayResponseUnit = unchecked((long) 0x01220003);
    public const long KDU_TIFFTAG_ImageHeight16 = unchecked((long) 0x01010003);
    public const long KDU_TIFFTAG_ImageHeight32 = unchecked((long) 0x01010004);
    public const long KDU_TIFFTAG_ImageWidth16 = unchecked((long) 0x01000003);
    public const long KDU_TIFFTAG_ImageWidth32 = unchecked((long) 0x01000004);
    public const long KDU_TIFFTAG_InkSet = unchecked((long) 0x014C0003);
    public const int KDU_TIFF_InkSet_CMYK = unchecked((int) 1);
    public const int KDU_TIFF_InkSet_NotCMYK = unchecked((int) 2);
    public const long KDU_TIFFTAG_MaxSampleValue = unchecked((long) 0x01190003);
    public const long KDU_TIFFTAG_MinSampleValue = unchecked((long) 0x01180003);
    public const long KDU_TIFFTAG_NumberOfInks = unchecked((long) 0x014E0003);
    public const long KDU_TIFFTAG_PhotometricInterp = unchecked((long) 0x01060003);
    public const int KDU_TIFF_PhotometricInterp_WHITEISZERO = unchecked((int) 0);
    public const int KDU_TIFF_PhotometricInterp_BLACKISZERO = unchecked((int) 1);
    public const int KDU_TIFF_PhotometricInterp_RGB = unchecked((int) 2);
    public const int KDU_TIFF_PhotometricInterp_PALETTE = unchecked((int) 3);
    public const int KDU_TIFF_PhotometricInterp_TRANSPARENCY = unchecked((int) 4);
    public const int KDU_TIFF_PhotometricInterp_SEPARATED = unchecked((int) 5);
    public const int KDU_TIFF_PhotometricInterp_YCbCr = unchecked((int) 6);
    public const int KDU_TIFF_PhotometricInterp_CIELab = unchecked((int) 8);
    public const long KDU_TIFFTAG_PlanarConfig = unchecked((long) 0x011C0003);
    public const int KDU_TIFF_PlanarConfig_CONTIG = unchecked((int) 1);
    public const int KDU_TIFF_PlanarConfig_PLANAR = unchecked((int) 2);
    public const long KDU_TIFFTAG_ResolutionUnit = unchecked((long) 0x01280003);
    public const int KDU_TIFF_ResolutionUnit_NONE = unchecked((int) 1);
    public const int KDU_TIFF_ResolutionUnit_INCH = unchecked((int) 2);
    public const int KDU_TIFF_ResolutionUnit_CM = unchecked((int) 3);
    public const long KDU_TIFFTAG_RowsPerStrip16 = unchecked((long) 0x01160003);
    public const long KDU_TIFFTAG_RowsPerStrip32 = unchecked((long) 0x01160004);
    public const long KDU_TIFFTAG_SampleFormat = unchecked((long) 0x01530003);
    public const int KDU_TIFF_SampleFormat_UNSIGNED = unchecked((int) 1);
    public const int KDU_TIFF_SampleFormat_SIGNED = unchecked((int) 2);
    public const int KDU_TIFF_SampleFormat_FLOAT = unchecked((int) 3);
    public const int KDU_TIFF_SampleFormat_UNDEFINED = unchecked((int) 3);
    public const long KDU_TIFFTAG_SamplesPerPixel = unchecked((long) 0x01150003);
    public const long KDU_TIFFTAG_SminSampleValueF = unchecked((long) 0x0154000B);
    public const long KDU_TIFFTAG_SminSampleValueD = unchecked((long) 0x0154000C);
    public const long KDU_TIFFTAG_SmaxSampleValueF = unchecked((long) 0x0155000B);
    public const long KDU_TIFFTAG_SmaxSampleValueD = unchecked((long) 0x0155000C);
    public const long KDU_TIFFTAG_StripByteCounts16 = unchecked((long) 0x01170003);
    public const long KDU_TIFFTAG_StripByteCounts32 = unchecked((long) 0x01170004);
    public const long KDU_TIFFTAG_StripOffsets16 = unchecked((long) 0x01110003);
    public const long KDU_TIFFTAG_StripOffsets32 = unchecked((long) 0x01110004);
    public const long KDU_TIFFTAG_TileWidth16 = unchecked((long) 0x01420003);
    public const long KDU_TIFFTAG_TileWidth32 = unchecked((long) 0x01420004);
    public const long KDU_TIFFTAG_TileHeight16 = unchecked((long) 0x01430003);
    public const long KDU_TIFFTAG_TileHeight32 = unchecked((long) 0x01430004);
    public const long KDU_TIFFTAG_TileOffsets = unchecked((long) 0x01440004);
    public const long KDU_TIFFTAG_XResolution = unchecked((long) 0x011A0005);
    public const long KDU_TIFFTAG_YResolution = unchecked((long) 0x011B0005);
    public const int KDU_COMPOSITOR_SCALE_TOO_SMALL = unchecked((int) 1);
    public const int KDU_COMPOSITOR_CANNOT_FLIP = unchecked((int) 2);
    public const long KDU_SIMPLE_VIDEO_YCC = unchecked((long) 1);
    public const long KDU_SIMPLE_VIDEO_RGB = unchecked((long) 2);
    public const long jp2_signature_4cc = unchecked((long) 0x6a502020);
    public const long jp2_file_type_4cc = unchecked((long) 0x66747970);
    public const long jp2_header_4cc = unchecked((long) 0x6a703268);
    public const long jp2_image_header_4cc = unchecked((long) 0x69686472);
    public const long jp2_bits_per_component_4cc = unchecked((long) 0x62706363);
    public const long jp2_colour_4cc = unchecked((long) 0x636f6c72);
    public const long jp2_palette_4cc = unchecked((long) 0x70636c72);
    public const long jp2_component_mapping_4cc = unchecked((long) 0x636d6170);
    public const long jp2_channel_definition_4cc = unchecked((long) 0x63646566);
    public const long jp2_resolution_4cc = unchecked((long) 0x72657320);
    public const long jp2_capture_resolution_4cc = unchecked((long) 0x72657363);
    public const long jp2_display_resolution_4cc = unchecked((long) 0x72657364);
    public const long jp2_codestream_4cc = unchecked((long) 0x6a703263);
    public const long jp2_dtbl_4cc = unchecked((long) 0x6474626c);
    public const long jp2_data_entry_url_4cc = unchecked((long) 0x75726c20);
    public const long jp2_fragment_table_4cc = unchecked((long) 0x6674626c);
    public const long jp2_fragment_list_4cc = unchecked((long) 0x666c7374);
    public const long jp2_reader_requirements_4cc = unchecked((long) 0x72726571);
    public const long jp2_codestream_header_4cc = unchecked((long) 0x6a706368);
    public const long jp2_desired_reproductions_4cc = unchecked((long) 0x64726570);
    public const long jp2_compositing_layer_hdr_4cc = unchecked((long) 0x6a706c68);
    public const long jp2_registration_4cc = unchecked((long) 0x63726567);
    public const long jp2_opacity_4cc = unchecked((long) 0x6f706374);
    public const long jp2_colour_group_4cc = unchecked((long) 0x63677270);
    public const long jp2_composition_4cc = unchecked((long) 0x636f6d70);
    public const long jp2_comp_options_4cc = unchecked((long) 0x636f7074);
    public const long jp2_comp_instruction_set_4cc = unchecked((long) 0x696e7374);
    public const long jp2_iprights_4cc = unchecked((long) 0x6a703269);
    public const long jp2_uuid_4cc = unchecked((long) 0x75756964);
    public const long jp2_uuid_info_4cc = unchecked((long) 0x75696e66);
    public const long jp2_label_4cc = unchecked((long) 0x6c626c20);
    public const long jp2_xml_4cc = unchecked((long) 0x786d6c20);
    public const long jp2_number_list_4cc = unchecked((long) 0x6e6c7374);
    public const long jp2_roi_description_4cc = unchecked((long) 0x726f6964);
    public const long jp2_association_4cc = unchecked((long) 0x61736f63);
    public const long mj2_movie_4cc = unchecked((long) 0x6d6f6f76);
    public const long mj2_movie_header_4cc = unchecked((long) 0x6d766864);
    public const long mj2_track_4cc = unchecked((long) 0x7472616b);
    public const long mj2_track_header_4cc = unchecked((long) 0x746b6864);
    public const long mj2_media_4cc = unchecked((long) 0x6d646961);
    public const long mj2_media_header_4cc = unchecked((long) 0x6d646864);
    public const long mj2_media_header_typo_4cc = unchecked((long) 0x6d686472);
    public const long mj2_media_handler_4cc = unchecked((long) 0x68646c72);
    public const long mj2_media_information_4cc = unchecked((long) 0x6d696e66);
    public const long mj2_video_media_header_4cc = unchecked((long) 0x766d6864);
    public const long mj2_video_handler_4cc = unchecked((long) 0x76696465);
    public const long mj2_data_information_4cc = unchecked((long) 0x64696e66);
    public const long mj2_data_reference_4cc = unchecked((long) 0x64726566);
    public const long mj2_url_4cc = unchecked((long) 0x75726c20);
    public const long mj2_sample_table_4cc = unchecked((long) 0x7374626c);
    public const long mj2_sample_description_4cc = unchecked((long) 0x73747364);
    public const long mj2_visual_sample_entry_4cc = unchecked((long) 0x6d6a7032);
    public const long mj2_field_coding_4cc = unchecked((long) 0x6669656c);
    public const long mj2_sample_size_4cc = unchecked((long) 0x7374737a);
    public const long mj2_sample_to_chunk_4cc = unchecked((long) 0x73747363);
    public const long mj2_chunk_offset_4cc = unchecked((long) 0x7374636f);
    public const long mj2_chunk_offset64_4cc = unchecked((long) 0x636f3634);
    public const long mj2_time_to_sample_4cc = unchecked((long) 0x73747473);
    public const long jp2_mdat_4cc = unchecked((long) 0x6d646174);
    public const long jp2_free_4cc = unchecked((long) 0x66726565);
    public const long mj2_skip_4cc = unchecked((long) 0x736b6970);
    public const long jp2_placeholder_4cc = unchecked((long) 0x70686c64);
    public const int JP2_COMPRESSION_TYPE_NONE = unchecked((int) 0);
    public const int JP2_COMPRESSION_TYPE_MH = unchecked((int) 1);
    public const int JP2_COMPRESSION_TYPE_MR = unchecked((int) 2);
    public const int JP2_COMPRESSION_TYPE_MMR = unchecked((int) 3);
    public const int JP2_COMPRESSION_TYPE_JBIG_B = unchecked((int) 4);
    public const int JP2_COMPRESSION_TYPE_JPEG = unchecked((int) 5);
    public const int JP2_COMPRESSION_TYPE_JLS = unchecked((int) 6);
    public const int JP2_COMPRESSION_TYPE_JPEG2000 = unchecked((int) 7);
    public const int JP2_COMPRESSION_TYPE_JBIG2 = unchecked((int) 8);
    public const int JP2_COMPRESSION_TYPE_JBIG = unchecked((int) 9);
    public const int JP2_bilevel1_SPACE  = unchecked((int) 0);
    public const int JP2_YCbCr1_SPACE  = unchecked((int) 1);
    public const int JP2_YCbCr2_SPACE  = unchecked((int) 3);
    public const int JP2_YCbCr3_SPACE  = unchecked((int) 4);
    public const int JP2_PhotoYCC_SPACE  = unchecked((int) 9);
    public const int JP2_CMY_SPACE  = unchecked((int) 11);
    public const int JP2_CMYK_SPACE  = unchecked((int) 12);
    public const int JP2_YCCK_SPACE  = unchecked((int) 13);
    public const int JP2_CIELab_SPACE  = unchecked((int) 14);
    public const int JP2_bilevel2_SPACE  = unchecked((int) 15);
    public const int JP2_sRGB_SPACE  = unchecked((int) 16);
    public const int JP2_sLUM_SPACE  = unchecked((int) 17);
    public const int JP2_sYCC_SPACE  = unchecked((int) 18);
    public const int JP2_CIEJab_SPACE  = unchecked((int) 19);
    public const int JP2_esRGB_SPACE  = unchecked((int) 20);
    public const int JP2_ROMMRGB_SPACE  = unchecked((int) 21);
    public const int JP2_YPbPr60_SPACE  = unchecked((int) 22);
    public const int JP2_YPbPr50_SPACE  = unchecked((int) 23);
    public const int JP2_esYCC_SPACE  = unchecked((int) 24);
    public const int JP2_iccLUM_SPACE  = unchecked((int) 100);
    public const int JP2_iccRGB_SPACE  = unchecked((int) 101);
    public const int JP2_iccANY_SPACE  = unchecked((int) 102);
    public const int JP2_vendor_SPACE  = unchecked((int) 200);
    public const long JP2_CIE_D50 = unchecked((long) 0x00443530);
    public const long JP2_CIE_D65 = unchecked((long) 0x00443635);
    public const long JP2_CIE_D75 = unchecked((long) 0x00443735);
    public const long JP2_CIE_SA = unchecked((long) 0x00005341);
    public const long JP2_CIE_SC = unchecked((long) 0x00005343);
    public const long JP2_CIE_F2 = unchecked((long) 0x00004632);
    public const long JP2_CIE_F7 = unchecked((long) 0x00004637);
    public const long JP2_CIE_F11 = unchecked((long) 0x00463131);
    public const int JPX_SF_CODESTREAM_NO_EXTENSIONS = unchecked((int) 1);
    public const int JPX_SF_MULTIPLE_LAYERS = unchecked((int) 2);
    public const int JPX_SF_JPEG2000_PART1_PROFILE0 = unchecked((int) 3);
    public const int JPX_SF_JPEG2000_PART1_PROFILE1 = unchecked((int) 4);
    public const int JPX_SF_JPEG2000_PART1 = unchecked((int) 5);
    public const int JPX_SF_JPEG2000_PART2 = unchecked((int) 6);
    public const int JPX_SF_CODESTREAM_USING_DCT = unchecked((int) 7);
    public const int JPX_SF_NO_OPACITY = unchecked((int) 8);
    public const int JPX_SF_OPACITY_NOT_PREMULTIPLIED = unchecked((int) 9);
    public const int JPX_SF_OPACITY_PREMULTIPLIED = unchecked((int) 10);
    public const int JPX_SF_OPACITY_BY_CHROMA_KEY = unchecked((int) 11);
    public const int JPX_SF_CODESTREAM_CONTIGUOUS = unchecked((int) 12);
    public const int JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL_AND_ORDERED = unchecked((int) 13);
    public const int JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL = unchecked((int) 14);
    public const int JPX_SF_CODESTREAM_FRAGMENTS_LOCAL = unchecked((int) 15);
    public const int JPX_SF_CODESTREAM_FRAGMENTS_REMOTE = unchecked((int) 16);
    public const int JPX_SF_COMPOSITING_USED = unchecked((int) 17);
    public const int JPX_SF_COMPOSITING_NOT_REQUIRED = unchecked((int) 18);
    public const int JPX_SF_MULTIPLE_LAYERS_NO_COMPOSITING_OR_ANIMATION = unchecked((int) 19);
    public const int JPX_SF_ONE_CODESTREAM_PER_LAYER = unchecked((int) 20);
    public const int JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER = unchecked((int) 21);
    public const int JPX_SF_SINGLE_COLOUR_SPACE = unchecked((int) 22);
    public const int JPX_SF_MULTIPLE_COLOUR_SPACES = unchecked((int) 23);
    public const int JPX_SF_NO_ANIMATION = unchecked((int) 24);
    public const int JPX_SF_ANIMATED_COVERED_BY_FIRST_LAYER = unchecked((int) 25);
    public const int JPX_SF_ANIMATED_NOT_COVERED_BY_FIRST_LAYER = unchecked((int) 26);
    public const int JPX_SF_ANIMATED_LAYERS_NOT_REUSED = unchecked((int) 27);
    public const int JPX_SF_ANIMATED_LAYERS_REUSED = unchecked((int) 28);
    public const int JPX_SF_ANIMATED_PERSISTENT_FRAMES = unchecked((int) 29);
    public const int JPX_SF_ANIMATED_NON_PERSISTENT_FRAMES = unchecked((int) 30);
    public const int JPX_SF_NO_SCALING = unchecked((int) 31);
    public const int JPX_SF_SCALING_WITHIN_LAYER = unchecked((int) 32);
    public const int JPX_SF_SCALING_BETWEEN_LAYERS = unchecked((int) 33);
    public const int JPX_SF_ROI_METADATA = unchecked((int) 34);
    public const int JPX_SF_IPR_METADATA = unchecked((int) 35);
    public const int JPX_SF_CONTENT_METADATA = unchecked((int) 36);
    public const int JPX_SF_HISTORY_METADATA = unchecked((int) 37);
    public const int JPX_SF_CREATION_METADATA = unchecked((int) 38);
    public const int JPX_SF_DIGITALLY_SIGNED = unchecked((int) 39);
    public const int JPX_SF_CHECKSUMMED = unchecked((int) 40);
    public const int JPX_SF_DESIRED_REPRODUCTION = unchecked((int) 41);
    public const int JPX_SF_PALETTIZED_COLOUR = unchecked((int) 42);
    public const int JPX_SF_RESTRICTED_ICC = unchecked((int) 43);
    public const int JPX_SF_ANY_ICC = unchecked((int) 44);
    public const int JPX_SF_sRGB = unchecked((int) 45);
    public const int JPX_SF_sLUM = unchecked((int) 46);
    public const int JPX_SF_sYCC = unchecked((int) 70);
    public const int JPX_SF_BILEVEL1 = unchecked((int) 47);
    public const int JPX_SF_BILEVEL2 = unchecked((int) 48);
    public const int JPX_SF_YCbCr1 = unchecked((int) 49);
    public const int JPX_SF_YCbCr2 = unchecked((int) 50);
    public const int JPX_SF_YCbCr3 = unchecked((int) 51);
    public const int JPX_SF_PhotoYCC = unchecked((int) 52);
    public const int JPX_SF_YCCK = unchecked((int) 53);
    public const int JPX_SF_CMY = unchecked((int) 54);
    public const int JPX_SF_CMYK = unchecked((int) 55);
    public const int JPX_SF_LAB_DEFAULT = unchecked((int) 56);
    public const int JPX_SF_LAB = unchecked((int) 57);
    public const int JPX_SF_JAB_DEFAULT = unchecked((int) 58);
    public const int JPX_SF_JAB = unchecked((int) 59);
    public const int JPX_SF_esRGB = unchecked((int) 60);
    public const int JPX_SF_ROMMRGB = unchecked((int) 61);
    public const int JPX_SF_SAMPLES_NOT_SQUARE = unchecked((int) 62);
    public const int JPX_SF_LAYERS_HAVE_LABELS = unchecked((int) 63);
    public const int JPX_SF_CODESTREAMS_HAVE_LABELS = unchecked((int) 64);
    public const int JPX_SF_MULTIPLE_COLOUR_SPACES2 = unchecked((int) 65);
    public const int JPX_SF_LAYERS_HAVE_DIFFERENT_METADATA = unchecked((int) 66);
    public const short MJ2_GRAPHICS_COPY = unchecked((short) 0x0000);
    public const short MJ2_GRAPHICS_BLUE_SCREEN = unchecked((short) 0x0024);
    public const short MJ2_GRAPHICS_ALPHA = unchecked((short) 0x0100);
    public const short MJ2_GRAPHICS_PREMULT_ALPHA = unchecked((short) 0x0101);
    public const short MJ2_GRAPHICS_COMPONENT_ALPHA = unchecked((short) 0x0110);
    public const int MJ2_TRACK_NON_EXISTENT = unchecked((int) 0);
    public const int MJ2_TRACK_MAY_EXIST = unchecked((int) -1);
    public const int MJ2_TRACK_IS_VIDEO = unchecked((int) 1);
    public const int MJ2_TRACK_IS_OTHER = unchecked((int) 1000);
    public const int KDU_JPIP_CONTEXT_NONE = unchecked((int) 0);
    public const int KDU_JPIP_CONTEXT_JPXL = unchecked((int) 1);
    public const int KDU_JPIP_CONTEXT_MJ2T = unchecked((int) 2);
    public const int KDU_JPIP_CONTEXT_TRANSLATED = unchecked((int) -1);
    public const int KDU_MRQ_ALL = unchecked((int) 1);
    public const int KDU_MRQ_GLOBAL = unchecked((int) 2);
    public const int KDU_MRQ_STREAM = unchecked((int) 4);
    public const int KDU_MRQ_WINDOW = unchecked((int) 8);
    public const int KDU_MRQ_DEFAULT = unchecked((int) 14);
    public const int KDU_MRQ_ANY = unchecked((int) 15);
    public const int KDS_METASCOPE_MANDATORY = unchecked((int) 0x00001);
    public const int KDS_METASCOPE_IMAGE_MANDATORY = unchecked((int) 0x00002);
    public const int KDS_METASCOPE_INCLUDE_NEXT_SIBLING = unchecked((int) 0x00010);
    public const int KDS_METASCOPE_LEAF = unchecked((int) 0x00020);
    public const int KDS_METASCOPE_HAS_GLOBAL_DATA = unchecked((int) 0x00100);
    public const int KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA = unchecked((int) 0x00200);
    public const int KDS_METASCOPE_HAS_IMAGE_WIDE_DATA = unchecked((int) 0x01000);
    public const int KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA = unchecked((int) 0x02000);
  }
}
