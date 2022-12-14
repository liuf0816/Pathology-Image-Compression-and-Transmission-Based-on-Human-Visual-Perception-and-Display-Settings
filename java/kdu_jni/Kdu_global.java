package kdu_jni;

public class Kdu_global {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public native static int Ceil_ratio(int _num, int _den) throws KduException;
  public native static int Floor_ratio(int _num, int _den) throws KduException;
  public native static long Jp2_4cc_to_int(String _string) throws KduException;
  public native static boolean Jp2_is_superbox(long _box_type) throws KduException;
  public native static void Kdu_convert_rgb_to_ycc(Kdu_line_buf _c1, Kdu_line_buf _c2, Kdu_line_buf _c3) throws KduException;
  public native static void Kdu_convert_ycc_to_rgb(Kdu_line_buf _c1, Kdu_line_buf _c2, Kdu_line_buf _c3, int _width) throws KduException;
  public void Kdu_convert_ycc_to_rgb(Kdu_line_buf _c1, Kdu_line_buf _c2, Kdu_line_buf _c3) throws KduException
  {
    Kdu_convert_ycc_to_rgb(_c1,_c2,_c3,(int) -1);
  }
  public native static void Kdu_customize_errors(Kdu_message _handler) throws KduException;
  public native static void Kdu_customize_text(String _context, long _id, String _lead_in, String _text) throws KduException;
  public native static void Kdu_customize_text(String _context, long _id, int[] _lead_in, int[] _text) throws KduException;
  public native static void Kdu_customize_warnings(Kdu_message _handler) throws KduException;
  public native static int Kdu_fseek(long _fp, long _offset) throws KduException;
  public native static int Kdu_fseek(long _fp, long _offset, int _origin) throws KduException;
  public native static long Kdu_ftell(long _fp) throws KduException;
  public native static String Kdu_get_core_version() throws KduException;
  public native static int Kdu_get_num_processors() throws KduException;
  public native static void Kdu_print_error(String _message) throws KduException;
  public native static void Kdu_print_warning(String _message) throws KduException;
  public native static int Long_ceil_ratio(long _num, long _den) throws KduException;
  public native static int Long_floor_ratio(long _num, long _den) throws KduException;

  public static final short KDU_INT16_MAX = (short) 0x7FFF;
  public static final short KDU_INT16_MIN = (short) 0x8000;
  public static final int KDU_INT32_MAX = (int) 0x7FFFFFFF;
  public static final int KDU_INT32_MIN = (int) 0x80000000;
  public static final int LL_BAND = (int) 0;
  public static final int HL_BAND = (int) 1;
  public static final int LH_BAND = (int) 2;
  public static final int HH_BAND = (int) 3;
  public static final int KDU_FIX_POINT = (int) 13;
  public static final byte KD_LINE_BUF_ABSOLUTE = (byte) 1;
  public static final byte KD_LINE_BUF_SHORTS = (byte) 2;
  public static final String KDU_CORE_VERSION = (String) "v6.1";
  public static final int KDU_SOC = (int) 0xFF4F;
  public static final int KDU_SOT = (int) 0xFF90;
  public static final int KDU_SOD = (int) 0xFF93;
  public static final int KDU_SOP = (int) 0xFF91;
  public static final int KDU_EPH = (int) 0xFF92;
  public static final int KDU_EOC = (int) 0xFFD9;
  public static final int KDU_SIZ = (int) 0xFF51;
  public static final int KDU_CBD = (int) 0xFF78;
  public static final int KDU_MCT = (int) 0xFF74;
  public static final int KDU_MCC = (int) 0xFF75;
  public static final int KDU_MCO = (int) 0xFF77;
  public static final int KDU_COD = (int) 0xFF52;
  public static final int KDU_COC = (int) 0xFF53;
  public static final int KDU_QCD = (int) 0xFF5C;
  public static final int KDU_QCC = (int) 0xFF5D;
  public static final int KDU_RGN = (int) 0xFF5E;
  public static final int KDU_POC = (int) 0xFF5F;
  public static final int KDU_CRG = (int) 0xFF63;
  public static final int KDU_DFS = (int) 0xFF72;
  public static final int KDU_ADS = (int) 0xFF73;
  public static final int KDU_ATK = (int) 0xFF79;
  public static final int KDU_COM = (int) 0xFF64;
  public static final int KDU_TLM = (int) 0xFF55;
  public static final int KDU_PLM = (int) 0xFF57;
  public static final int KDU_PLT = (int) 0xFF58;
  public static final int KDU_PPM = (int) 0xFF60;
  public static final int KDU_PPT = (int) 0xFF61;
  public static final int KDU_WANT_OUTPUT_COMPONENTS = (int) 0;
  public static final int KDU_WANT_CODESTREAM_COMPONENTS = (int) 1;
  public static final int KDU_SOURCE_CAP_SEQUENTIAL = (int) 0x0001;
  public static final int KDU_SOURCE_CAP_SEEKABLE = (int) 0x0002;
  public static final int KDU_SOURCE_CAP_CACHED = (int) 0x0004;
  public static final int KDU_SOURCE_CAP_IN_MEMORY = (int) 0x0008;
  public static final String SIZ_params = (String) "SIZ";
  public static final String Sprofile = (String) "Sprofile";
  public static final String Scap = (String) "Scap";
  public static final String Sextensions = (String) "Sextensions";
  public static final String Ssize = (String) "Ssize";
  public static final String Sorigin = (String) "Sorigin";
  public static final String Stiles = (String) "Stiles";
  public static final String Stile_origin = (String) "Stile_origin";
  public static final String Scomponents = (String) "Scomponents";
  public static final String Ssigned = (String) "Ssigned";
  public static final String Sprecision = (String) "Sprecision";
  public static final String Ssampling = (String) "Ssampling";
  public static final String Sdims = (String) "Sdims";
  public static final String Mcomponents = (String) "Mcomponents";
  public static final String Msigned = (String) "Msigned";
  public static final String Mprecision = (String) "Mprecision";
  public static final int Sprofile_PROFILE0 = (int) 0;
  public static final int Sprofile_PROFILE1 = (int) 1;
  public static final int Sprofile_PROFILE2 = (int) 2;
  public static final int Sprofile_PART2 = (int) 3;
  public static final int Sprofile_CINEMA2K = (int) 4;
  public static final int Sprofile_CINEMA4K = (int) 5;
  public static final int Sextensions_DC = (int) 1;
  public static final int Sextensions_VARQ = (int) 2;
  public static final int Sextensions_TCQ = (int) 4;
  public static final int Sextensions_PRECQ = (int) 2048;
  public static final int Sextensions_VIS = (int) 8;
  public static final int Sextensions_SSO = (int) 16;
  public static final int Sextensions_DECOMP = (int) 32;
  public static final int Sextensions_ANY_KNL = (int) 64;
  public static final int Sextensions_SYM_KNL = (int) 128;
  public static final int Sextensions_MCT = (int) 256;
  public static final int Sextensions_CURVE = (int) 512;
  public static final int Sextensions_ROI = (int) 1024;
  public static final String MCT_params = (String) "MCT";
  public static final String Mmatrix_size = (String) "Mmatrix_size";
  public static final String Mmatrix_coeffs = (String) "Mmatrix_coeffs";
  public static final String Mvector_size = (String) "Mvector_size";
  public static final String Mvector_coeffs = (String) "Mvector_coeffs";
  public static final String Mtriang_size = (String) "Mtriang_size";
  public static final String Mtriang_coeffs = (String) "Mtriang_coeffs";
  public static final String MCC_params = (String) "MCC";
  public static final String Mstage_inputs = (String) "Mstage_inputs";
  public static final String Mstage_outputs = (String) "Mstage_outputs";
  public static final String Mstage_blocks = (String) "Mstage_collections";
  public static final String Mstage_xforms = (String) "Mstage_xforms";
  public static final int Mxform_DEP = (int) 0;
  public static final int Mxform_MATRIX = (int) 9;
  public static final int Mxform_DWT = (int) 3;
  public static final int Mxform_MAT = (int) 1000;
  public static final String MCO_params = (String) "MCO";
  public static final String Mnum_stages = (String) "Mnum_stages";
  public static final String Mstages = (String) "Mstages";
  public static final String ATK_params = (String) "ATK";
  public static final String Kreversible = (String) "Kreversible";
  public static final String Ksymmetric = (String) "Ksymmetric";
  public static final String Kextension = (String) "Kextension";
  public static final String Ksteps = (String) "Ksteps";
  public static final String Kcoeffs = (String) "Kcoeffs";
  public static final int Kextension_CON = (int) 0;
  public static final int Kextension_SYM = (int) 1;
  public static final String COD_params = (String) "COD";
  public static final String Cycc = (String) "Cycc";
  public static final String Cmct = (String) "Cmct";
  public static final String Clayers = (String) "Clayers";
  public static final String Cuse_sop = (String) "Cuse_sop";
  public static final String Cuse_eph = (String) "Cuse_eph";
  public static final String Corder = (String) "Corder";
  public static final String Calign_blk_last = (String) "Calign_blk_last";
  public static final String Clevels = (String) "Clevels";
  public static final String Cads = (String) "Cads";
  public static final String Cdfs = (String) "Cdfs";
  public static final String Cdecomp = (String) "Cdecomp";
  public static final String Creversible = (String) "Creversible";
  public static final String Ckernels = (String) "Ckernels";
  public static final String Catk = (String) "Catk";
  public static final String Cuse_precincts = (String) "Cuse_precincts";
  public static final String Cprecincts = (String) "Cprecincts";
  public static final String Cblk = (String) "Cblk";
  public static final String Cmodes = (String) "Cmodes";
  public static final String Cweight = (String) "Cweight";
  public static final String Clev_weights = (String) "Clev_weights";
  public static final String Cband_weights = (String) "Cband_weights";
  public static final String Creslengths = (String) "Creslengths";
  public static final int Cmct_ARRAY = (int) 2;
  public static final int Cmct_DWT = (int) 4;
  public static final int Corder_LRCP = (int) 0;
  public static final int Corder_RLCP = (int) 1;
  public static final int Corder_RPCL = (int) 2;
  public static final int Corder_PCRL = (int) 3;
  public static final int Corder_CPRL = (int) 4;
  public static final int Ckernels_W9X7 = (int) 0;
  public static final int Ckernels_W5X3 = (int) 1;
  public static final int Ckernels_ATK = (int) -1;
  public static final int Cmodes_BYPASS = (int) 1;
  public static final int Cmodes_RESET = (int) 2;
  public static final int Cmodes_RESTART = (int) 4;
  public static final int Cmodes_CAUSAL = (int) 8;
  public static final int Cmodes_ERTERM = (int) 16;
  public static final int Cmodes_SEGMARK = (int) 32;
  public static final String ADS_params = (String) "ADS";
  public static final String Ddecomp = (String) "Ddecomp";
  public static final String DOads = (String) "DOads";
  public static final String DSads = (String) "DSads";
  public static final String DFS_params = (String) "DFS";
  public static final String DSdfs = (String) "DSdfs";
  public static final String QCD_params = (String) "QCD";
  public static final String Qguard = (String) "Qguard";
  public static final String Qderived = (String) "Qderived";
  public static final String Qabs_steps = (String) "Qabs_steps";
  public static final String Qabs_ranges = (String) "Qabs_ranges";
  public static final String Qstep = (String) "Qstep";
  public static final String RGN_params = (String) "RGN";
  public static final String Rshift = (String) "Rshift";
  public static final String Rlevels = (String) "Rlevels";
  public static final String Rweight = (String) "Rweight";
  public static final String POC_params = (String) "POC";
  public static final String Porder = (String) "Porder";
  public static final String CRG_params = (String) "CRG";
  public static final String CRGoffset = (String) "CRGoffset";
  public static final String ORG_params = (String) "ORG";
  public static final String ORGtparts = (String) "ORGtparts";
  public static final String ORGgen_tlm = (String) "ORGgen_tlm";
  public static final String ORGgen_plt = (String) "ORGgen_plt";
  public static final int ORGtparts_R = (int) 1;
  public static final int ORGtparts_L = (int) 2;
  public static final int ORGtparts_C = (int) 4;
  public static final int KDU_ANALYSIS_LOW = (int) 0;
  public static final int KDU_ANALYSIS_HIGH = (int) 1;
  public static final int KDU_SYNTHESIS_LOW = (int) 2;
  public static final int KDU_SYNTHESIS_HIGH = (int) 3;
  public static final long KDU_TIFFTAG_Artist = (long) 0x013B0002;
  public static final long KDU_TIFFTAG_Copyright = (long) 0x82980002;
  public static final long KDU_TIFFTAG_HostComputer = (long) 0x013C0002;
  public static final long KDU_TIFFTAG_ImageDescription = (long) 0x010E0002;
  public static final long KDU_TIFFTAG_Make = (long) 0x010F0002;
  public static final long KDU_TIFFTAG_Model = (long) 0x01100002;
  public static final long KDU_TIFFTAG_Software = (long) 0x01310002;
  public static final long KDU_TIFFTAG_BitsPerSample = (long) 0x01020003;
  public static final long KDU_TIFFTAG_ColorMap = (long) 0x01400003;
  public static final long KDU_TIFFTAG_Compression = (long) 0x01030003;
  public static final int KDU_TIFF_Compression_NONE = (int) 1;
  public static final int KDU_TIFF_Compression_CCITT = (int) 2;
  public static final int KDU_TIFF_Compression_PACKBITS = (int) 32773;
  public static final long KDU_TIFFTAG_ExtraSamples = (long) 0x01520003;
  public static final int KDU_TIFF_ExtraSamples_UNDEFINED = (int) 0;
  public static final int KDU_TIFF_ExtraSamples_PREMULTIPLIED_ALPHA = (int) 1;
  public static final int KDU_TIFF_ExtraSamples_UNASSOCIATED_ALPHA = (int) 1;
  public static final long KDU_TIFFTAG_FillOrder = (long) 0x010A0003;
  public static final int KDU_TIFF_FillOrder_MSB_FIRST = (int) 1;
  public static final int KDU_TIFF_FillOrder_LSB_FIRST = (int) 2;
  public static final long KDU_TIFFTAG_GrayResponseCurve = (long) 0x01230003;
  public static final long KDU_TIFFTAG_GrayResponseUnit = (long) 0x01220003;
  public static final long KDU_TIFFTAG_ImageHeight16 = (long) 0x01010003;
  public static final long KDU_TIFFTAG_ImageHeight32 = (long) 0x01010004;
  public static final long KDU_TIFFTAG_ImageWidth16 = (long) 0x01000003;
  public static final long KDU_TIFFTAG_ImageWidth32 = (long) 0x01000004;
  public static final long KDU_TIFFTAG_InkSet = (long) 0x014C0003;
  public static final int KDU_TIFF_InkSet_CMYK = (int) 1;
  public static final int KDU_TIFF_InkSet_NotCMYK = (int) 2;
  public static final long KDU_TIFFTAG_MaxSampleValue = (long) 0x01190003;
  public static final long KDU_TIFFTAG_MinSampleValue = (long) 0x01180003;
  public static final long KDU_TIFFTAG_NumberOfInks = (long) 0x014E0003;
  public static final long KDU_TIFFTAG_PhotometricInterp = (long) 0x01060003;
  public static final int KDU_TIFF_PhotometricInterp_WHITEISZERO = (int) 0;
  public static final int KDU_TIFF_PhotometricInterp_BLACKISZERO = (int) 1;
  public static final int KDU_TIFF_PhotometricInterp_RGB = (int) 2;
  public static final int KDU_TIFF_PhotometricInterp_PALETTE = (int) 3;
  public static final int KDU_TIFF_PhotometricInterp_TRANSPARENCY = (int) 4;
  public static final int KDU_TIFF_PhotometricInterp_SEPARATED = (int) 5;
  public static final int KDU_TIFF_PhotometricInterp_YCbCr = (int) 6;
  public static final int KDU_TIFF_PhotometricInterp_CIELab = (int) 8;
  public static final long KDU_TIFFTAG_PlanarConfig = (long) 0x011C0003;
  public static final int KDU_TIFF_PlanarConfig_CONTIG = (int) 1;
  public static final int KDU_TIFF_PlanarConfig_PLANAR = (int) 2;
  public static final long KDU_TIFFTAG_ResolutionUnit = (long) 0x01280003;
  public static final int KDU_TIFF_ResolutionUnit_NONE = (int) 1;
  public static final int KDU_TIFF_ResolutionUnit_INCH = (int) 2;
  public static final int KDU_TIFF_ResolutionUnit_CM = (int) 3;
  public static final long KDU_TIFFTAG_RowsPerStrip16 = (long) 0x01160003;
  public static final long KDU_TIFFTAG_RowsPerStrip32 = (long) 0x01160004;
  public static final long KDU_TIFFTAG_SampleFormat = (long) 0x01530003;
  public static final int KDU_TIFF_SampleFormat_UNSIGNED = (int) 1;
  public static final int KDU_TIFF_SampleFormat_SIGNED = (int) 2;
  public static final int KDU_TIFF_SampleFormat_FLOAT = (int) 3;
  public static final int KDU_TIFF_SampleFormat_UNDEFINED = (int) 3;
  public static final long KDU_TIFFTAG_SamplesPerPixel = (long) 0x01150003;
  public static final long KDU_TIFFTAG_SminSampleValueF = (long) 0x0154000B;
  public static final long KDU_TIFFTAG_SminSampleValueD = (long) 0x0154000C;
  public static final long KDU_TIFFTAG_SmaxSampleValueF = (long) 0x0155000B;
  public static final long KDU_TIFFTAG_SmaxSampleValueD = (long) 0x0155000C;
  public static final long KDU_TIFFTAG_StripByteCounts16 = (long) 0x01170003;
  public static final long KDU_TIFFTAG_StripByteCounts32 = (long) 0x01170004;
  public static final long KDU_TIFFTAG_StripOffsets16 = (long) 0x01110003;
  public static final long KDU_TIFFTAG_StripOffsets32 = (long) 0x01110004;
  public static final long KDU_TIFFTAG_TileWidth16 = (long) 0x01420003;
  public static final long KDU_TIFFTAG_TileWidth32 = (long) 0x01420004;
  public static final long KDU_TIFFTAG_TileHeight16 = (long) 0x01430003;
  public static final long KDU_TIFFTAG_TileHeight32 = (long) 0x01430004;
  public static final long KDU_TIFFTAG_TileOffsets = (long) 0x01440004;
  public static final long KDU_TIFFTAG_XResolution = (long) 0x011A0005;
  public static final long KDU_TIFFTAG_YResolution = (long) 0x011B0005;
  public static final int KDU_COMPOSITOR_SCALE_TOO_SMALL = (int) 1;
  public static final int KDU_COMPOSITOR_CANNOT_FLIP = (int) 2;
  public static final long KDU_SIMPLE_VIDEO_YCC = (long) 1;
  public static final long KDU_SIMPLE_VIDEO_RGB = (long) 2;
  public static final long jp2_signature_4cc = (long) 0x6a502020;
  public static final long jp2_file_type_4cc = (long) 0x66747970;
  public static final long jp2_header_4cc = (long) 0x6a703268;
  public static final long jp2_image_header_4cc = (long) 0x69686472;
  public static final long jp2_bits_per_component_4cc = (long) 0x62706363;
  public static final long jp2_colour_4cc = (long) 0x636f6c72;
  public static final long jp2_palette_4cc = (long) 0x70636c72;
  public static final long jp2_component_mapping_4cc = (long) 0x636d6170;
  public static final long jp2_channel_definition_4cc = (long) 0x63646566;
  public static final long jp2_resolution_4cc = (long) 0x72657320;
  public static final long jp2_capture_resolution_4cc = (long) 0x72657363;
  public static final long jp2_display_resolution_4cc = (long) 0x72657364;
  public static final long jp2_codestream_4cc = (long) 0x6a703263;
  public static final long jp2_dtbl_4cc = (long) 0x6474626c;
  public static final long jp2_data_entry_url_4cc = (long) 0x75726c20;
  public static final long jp2_fragment_table_4cc = (long) 0x6674626c;
  public static final long jp2_fragment_list_4cc = (long) 0x666c7374;
  public static final long jp2_reader_requirements_4cc = (long) 0x72726571;
  public static final long jp2_codestream_header_4cc = (long) 0x6a706368;
  public static final long jp2_desired_reproductions_4cc = (long) 0x64726570;
  public static final long jp2_compositing_layer_hdr_4cc = (long) 0x6a706c68;
  public static final long jp2_registration_4cc = (long) 0x63726567;
  public static final long jp2_opacity_4cc = (long) 0x6f706374;
  public static final long jp2_colour_group_4cc = (long) 0x63677270;
  public static final long jp2_composition_4cc = (long) 0x636f6d70;
  public static final long jp2_comp_options_4cc = (long) 0x636f7074;
  public static final long jp2_comp_instruction_set_4cc = (long) 0x696e7374;
  public static final long jp2_iprights_4cc = (long) 0x6a703269;
  public static final long jp2_uuid_4cc = (long) 0x75756964;
  public static final long jp2_uuid_info_4cc = (long) 0x75696e66;
  public static final long jp2_label_4cc = (long) 0x6c626c20;
  public static final long jp2_xml_4cc = (long) 0x786d6c20;
  public static final long jp2_number_list_4cc = (long) 0x6e6c7374;
  public static final long jp2_roi_description_4cc = (long) 0x726f6964;
  public static final long jp2_association_4cc = (long) 0x61736f63;
  public static final long mj2_movie_4cc = (long) 0x6d6f6f76;
  public static final long mj2_movie_header_4cc = (long) 0x6d766864;
  public static final long mj2_track_4cc = (long) 0x7472616b;
  public static final long mj2_track_header_4cc = (long) 0x746b6864;
  public static final long mj2_media_4cc = (long) 0x6d646961;
  public static final long mj2_media_header_4cc = (long) 0x6d646864;
  public static final long mj2_media_header_typo_4cc = (long) 0x6d686472;
  public static final long mj2_media_handler_4cc = (long) 0x68646c72;
  public static final long mj2_media_information_4cc = (long) 0x6d696e66;
  public static final long mj2_video_media_header_4cc = (long) 0x766d6864;
  public static final long mj2_video_handler_4cc = (long) 0x76696465;
  public static final long mj2_data_information_4cc = (long) 0x64696e66;
  public static final long mj2_data_reference_4cc = (long) 0x64726566;
  public static final long mj2_url_4cc = (long) 0x75726c20;
  public static final long mj2_sample_table_4cc = (long) 0x7374626c;
  public static final long mj2_sample_description_4cc = (long) 0x73747364;
  public static final long mj2_visual_sample_entry_4cc = (long) 0x6d6a7032;
  public static final long mj2_field_coding_4cc = (long) 0x6669656c;
  public static final long mj2_sample_size_4cc = (long) 0x7374737a;
  public static final long mj2_sample_to_chunk_4cc = (long) 0x73747363;
  public static final long mj2_chunk_offset_4cc = (long) 0x7374636f;
  public static final long mj2_chunk_offset64_4cc = (long) 0x636f3634;
  public static final long mj2_time_to_sample_4cc = (long) 0x73747473;
  public static final long jp2_mdat_4cc = (long) 0x6d646174;
  public static final long jp2_free_4cc = (long) 0x66726565;
  public static final long mj2_skip_4cc = (long) 0x736b6970;
  public static final long jp2_placeholder_4cc = (long) 0x70686c64;
  public static final int JP2_COMPRESSION_TYPE_NONE = (int) 0;
  public static final int JP2_COMPRESSION_TYPE_MH = (int) 1;
  public static final int JP2_COMPRESSION_TYPE_MR = (int) 2;
  public static final int JP2_COMPRESSION_TYPE_MMR = (int) 3;
  public static final int JP2_COMPRESSION_TYPE_JBIG_B = (int) 4;
  public static final int JP2_COMPRESSION_TYPE_JPEG = (int) 5;
  public static final int JP2_COMPRESSION_TYPE_JLS = (int) 6;
  public static final int JP2_COMPRESSION_TYPE_JPEG2000 = (int) 7;
  public static final int JP2_COMPRESSION_TYPE_JBIG2 = (int) 8;
  public static final int JP2_COMPRESSION_TYPE_JBIG = (int) 9;
  public static final int JP2_bilevel1_SPACE  = (int) 0;
  public static final int JP2_YCbCr1_SPACE  = (int) 1;
  public static final int JP2_YCbCr2_SPACE  = (int) 3;
  public static final int JP2_YCbCr3_SPACE  = (int) 4;
  public static final int JP2_PhotoYCC_SPACE  = (int) 9;
  public static final int JP2_CMY_SPACE  = (int) 11;
  public static final int JP2_CMYK_SPACE  = (int) 12;
  public static final int JP2_YCCK_SPACE  = (int) 13;
  public static final int JP2_CIELab_SPACE  = (int) 14;
  public static final int JP2_bilevel2_SPACE  = (int) 15;
  public static final int JP2_sRGB_SPACE  = (int) 16;
  public static final int JP2_sLUM_SPACE  = (int) 17;
  public static final int JP2_sYCC_SPACE  = (int) 18;
  public static final int JP2_CIEJab_SPACE  = (int) 19;
  public static final int JP2_esRGB_SPACE  = (int) 20;
  public static final int JP2_ROMMRGB_SPACE  = (int) 21;
  public static final int JP2_YPbPr60_SPACE  = (int) 22;
  public static final int JP2_YPbPr50_SPACE  = (int) 23;
  public static final int JP2_esYCC_SPACE  = (int) 24;
  public static final int JP2_iccLUM_SPACE  = (int) 100;
  public static final int JP2_iccRGB_SPACE  = (int) 101;
  public static final int JP2_iccANY_SPACE  = (int) 102;
  public static final int JP2_vendor_SPACE  = (int) 200;
  public static final long JP2_CIE_D50 = (long) 0x00443530;
  public static final long JP2_CIE_D65 = (long) 0x00443635;
  public static final long JP2_CIE_D75 = (long) 0x00443735;
  public static final long JP2_CIE_SA = (long) 0x00005341;
  public static final long JP2_CIE_SC = (long) 0x00005343;
  public static final long JP2_CIE_F2 = (long) 0x00004632;
  public static final long JP2_CIE_F7 = (long) 0x00004637;
  public static final long JP2_CIE_F11 = (long) 0x00463131;
  public static final int JPX_SF_CODESTREAM_NO_EXTENSIONS = (int) 1;
  public static final int JPX_SF_MULTIPLE_LAYERS = (int) 2;
  public static final int JPX_SF_JPEG2000_PART1_PROFILE0 = (int) 3;
  public static final int JPX_SF_JPEG2000_PART1_PROFILE1 = (int) 4;
  public static final int JPX_SF_JPEG2000_PART1 = (int) 5;
  public static final int JPX_SF_JPEG2000_PART2 = (int) 6;
  public static final int JPX_SF_CODESTREAM_USING_DCT = (int) 7;
  public static final int JPX_SF_NO_OPACITY = (int) 8;
  public static final int JPX_SF_OPACITY_NOT_PREMULTIPLIED = (int) 9;
  public static final int JPX_SF_OPACITY_PREMULTIPLIED = (int) 10;
  public static final int JPX_SF_OPACITY_BY_CHROMA_KEY = (int) 11;
  public static final int JPX_SF_CODESTREAM_CONTIGUOUS = (int) 12;
  public static final int JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL_AND_ORDERED = (int) 13;
  public static final int JPX_SF_CODESTREAM_FRAGMENTS_INTERNAL = (int) 14;
  public static final int JPX_SF_CODESTREAM_FRAGMENTS_LOCAL = (int) 15;
  public static final int JPX_SF_CODESTREAM_FRAGMENTS_REMOTE = (int) 16;
  public static final int JPX_SF_COMPOSITING_USED = (int) 17;
  public static final int JPX_SF_COMPOSITING_NOT_REQUIRED = (int) 18;
  public static final int JPX_SF_MULTIPLE_LAYERS_NO_COMPOSITING_OR_ANIMATION = (int) 19;
  public static final int JPX_SF_ONE_CODESTREAM_PER_LAYER = (int) 20;
  public static final int JPX_SF_MULTIPLE_CODESTREAMS_PER_LAYER = (int) 21;
  public static final int JPX_SF_SINGLE_COLOUR_SPACE = (int) 22;
  public static final int JPX_SF_MULTIPLE_COLOUR_SPACES = (int) 23;
  public static final int JPX_SF_NO_ANIMATION = (int) 24;
  public static final int JPX_SF_ANIMATED_COVERED_BY_FIRST_LAYER = (int) 25;
  public static final int JPX_SF_ANIMATED_NOT_COVERED_BY_FIRST_LAYER = (int) 26;
  public static final int JPX_SF_ANIMATED_LAYERS_NOT_REUSED = (int) 27;
  public static final int JPX_SF_ANIMATED_LAYERS_REUSED = (int) 28;
  public static final int JPX_SF_ANIMATED_PERSISTENT_FRAMES = (int) 29;
  public static final int JPX_SF_ANIMATED_NON_PERSISTENT_FRAMES = (int) 30;
  public static final int JPX_SF_NO_SCALING = (int) 31;
  public static final int JPX_SF_SCALING_WITHIN_LAYER = (int) 32;
  public static final int JPX_SF_SCALING_BETWEEN_LAYERS = (int) 33;
  public static final int JPX_SF_ROI_METADATA = (int) 34;
  public static final int JPX_SF_IPR_METADATA = (int) 35;
  public static final int JPX_SF_CONTENT_METADATA = (int) 36;
  public static final int JPX_SF_HISTORY_METADATA = (int) 37;
  public static final int JPX_SF_CREATION_METADATA = (int) 38;
  public static final int JPX_SF_DIGITALLY_SIGNED = (int) 39;
  public static final int JPX_SF_CHECKSUMMED = (int) 40;
  public static final int JPX_SF_DESIRED_REPRODUCTION = (int) 41;
  public static final int JPX_SF_PALETTIZED_COLOUR = (int) 42;
  public static final int JPX_SF_RESTRICTED_ICC = (int) 43;
  public static final int JPX_SF_ANY_ICC = (int) 44;
  public static final int JPX_SF_sRGB = (int) 45;
  public static final int JPX_SF_sLUM = (int) 46;
  public static final int JPX_SF_sYCC = (int) 70;
  public static final int JPX_SF_BILEVEL1 = (int) 47;
  public static final int JPX_SF_BILEVEL2 = (int) 48;
  public static final int JPX_SF_YCbCr1 = (int) 49;
  public static final int JPX_SF_YCbCr2 = (int) 50;
  public static final int JPX_SF_YCbCr3 = (int) 51;
  public static final int JPX_SF_PhotoYCC = (int) 52;
  public static final int JPX_SF_YCCK = (int) 53;
  public static final int JPX_SF_CMY = (int) 54;
  public static final int JPX_SF_CMYK = (int) 55;
  public static final int JPX_SF_LAB_DEFAULT = (int) 56;
  public static final int JPX_SF_LAB = (int) 57;
  public static final int JPX_SF_JAB_DEFAULT = (int) 58;
  public static final int JPX_SF_JAB = (int) 59;
  public static final int JPX_SF_esRGB = (int) 60;
  public static final int JPX_SF_ROMMRGB = (int) 61;
  public static final int JPX_SF_SAMPLES_NOT_SQUARE = (int) 62;
  public static final int JPX_SF_LAYERS_HAVE_LABELS = (int) 63;
  public static final int JPX_SF_CODESTREAMS_HAVE_LABELS = (int) 64;
  public static final int JPX_SF_MULTIPLE_COLOUR_SPACES2 = (int) 65;
  public static final int JPX_SF_LAYERS_HAVE_DIFFERENT_METADATA = (int) 66;
  public static final short MJ2_GRAPHICS_COPY = (short) 0x0000;
  public static final short MJ2_GRAPHICS_BLUE_SCREEN = (short) 0x0024;
  public static final short MJ2_GRAPHICS_ALPHA = (short) 0x0100;
  public static final short MJ2_GRAPHICS_PREMULT_ALPHA = (short) 0x0101;
  public static final short MJ2_GRAPHICS_COMPONENT_ALPHA = (short) 0x0110;
  public static final int MJ2_TRACK_NON_EXISTENT = (int) 0;
  public static final int MJ2_TRACK_MAY_EXIST = (int) -1;
  public static final int MJ2_TRACK_IS_VIDEO = (int) 1;
  public static final int MJ2_TRACK_IS_OTHER = (int) 1000;
  public static final int KDU_JPIP_CONTEXT_NONE = (int) 0;
  public static final int KDU_JPIP_CONTEXT_JPXL = (int) 1;
  public static final int KDU_JPIP_CONTEXT_MJ2T = (int) 2;
  public static final int KDU_JPIP_CONTEXT_TRANSLATED = (int) -1;
  public static final int KDU_MRQ_ALL = (int) 1;
  public static final int KDU_MRQ_GLOBAL = (int) 2;
  public static final int KDU_MRQ_STREAM = (int) 4;
  public static final int KDU_MRQ_WINDOW = (int) 8;
  public static final int KDU_MRQ_DEFAULT = (int) 14;
  public static final int KDU_MRQ_ANY = (int) 15;
  public static final int KDS_METASCOPE_MANDATORY = (int) 0x00001;
  public static final int KDS_METASCOPE_IMAGE_MANDATORY = (int) 0x00002;
  public static final int KDS_METASCOPE_INCLUDE_NEXT_SIBLING = (int) 0x00010;
  public static final int KDS_METASCOPE_LEAF = (int) 0x00020;
  public static final int KDS_METASCOPE_HAS_GLOBAL_DATA = (int) 0x00100;
  public static final int KDS_METASCOPE_HAS_IMAGE_SPECIFIC_DATA = (int) 0x00200;
  public static final int KDS_METASCOPE_HAS_IMAGE_WIDE_DATA = (int) 0x01000;
  public static final int KDS_METASCOPE_HAS_REGION_SPECIFIC_DATA = (int) 0x02000;
}
