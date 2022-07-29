package kdu_jni;

public class Kdu_region_decompressor {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  protected Kdu_region_decompressor(long ptr) {
    _native_ptr = ptr;
  }
  public native void Native_destroy();
  public void finalize() {
    if ((_native_ptr & 1) != 0)
      { // Resource created and not donated
        Native_destroy();
      }
  }
  private static native long Native_create();
  public Kdu_region_decompressor() {
    this(Native_create());
  }
  public native Kdu_dims Get_rendered_image_dims(Kdu_codestream _codestream, Kdu_channel_mapping _mapping, int _single_component, int _discard_levels, Kdu_coords _expand_numerator, Kdu_coords _expand_denominator, int _access_mode) throws KduException;
  public native Kdu_dims Get_rendered_image_dims() throws KduException;
  public native void Set_white_stretch(int _white_stretch_precision) throws KduException;
  public native boolean Start(Kdu_codestream _codestream, Kdu_channel_mapping _mapping, int _single_component, int _discard_levels, int _max_layers, Kdu_dims _region, Kdu_coords _expand_numerator, Kdu_coords _expand_denominator, boolean _precise, int _access_mode, boolean _fastest, Kdu_thread_env _env, long _env_queue) throws KduException;
  public boolean Start(Kdu_codestream _codestream, Kdu_channel_mapping _mapping, int _single_component, int _discard_levels, int _max_layers, Kdu_dims _region, Kdu_coords _expand_numerator, Kdu_coords _expand_denominator, boolean _precise, int _access_mode) throws KduException
  {
    Kdu_thread_env env = null;
    return Start(_codestream,_mapping,_single_component,_discard_levels,_max_layers,_region,_expand_numerator,_expand_denominator,_precise,_access_mode,(boolean) false,env,0);
  }
  public boolean Start(Kdu_codestream _codestream, Kdu_channel_mapping _mapping, int _single_component, int _discard_levels, int _max_layers, Kdu_dims _region, Kdu_coords _expand_numerator, Kdu_coords _expand_denominator, boolean _precise, int _access_mode, boolean _fastest) throws KduException
  {
    Kdu_thread_env env = null;
    return Start(_codestream,_mapping,_single_component,_discard_levels,_max_layers,_region,_expand_numerator,_expand_denominator,_precise,_access_mode,_fastest,env,0);
  }
  public boolean Start(Kdu_codestream _codestream, Kdu_channel_mapping _mapping, int _single_component, int _discard_levels, int _max_layers, Kdu_dims _region, Kdu_coords _expand_numerator, Kdu_coords _expand_denominator, boolean _precise, int _access_mode, boolean _fastest, Kdu_thread_env _env) throws KduException
  {
    return Start(_codestream,_mapping,_single_component,_discard_levels,_max_layers,_region,_expand_numerator,_expand_denominator,_precise,_access_mode,_fastest,_env,0);
  }
  public native boolean Process(int[] _buffer, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region) throws KduException;
  public native boolean Process(byte[] _buffer, int[] _channel_offsets, int _pixel_gap, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region, int _precision_bits, boolean _measure_row_gap_in_pixels) throws KduException;
  public boolean Process(byte[] _buffer, int[] _channel_offsets, int _pixel_gap, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region) throws KduException
  {
    return Process(_buffer,_channel_offsets,_pixel_gap,_buffer_origin,_row_gap,_suggested_increment,_max_region_pixels,_incomplete_region,_new_region,(int) 8,(boolean) true);
  }
  public boolean Process(byte[] _buffer, int[] _channel_offsets, int _pixel_gap, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region, int _precision_bits) throws KduException
  {
    return Process(_buffer,_channel_offsets,_pixel_gap,_buffer_origin,_row_gap,_suggested_increment,_max_region_pixels,_incomplete_region,_new_region,_precision_bits,(boolean) true);
  }
  public native boolean Process(int[] _buffer, int[] _channel_offsets, int _pixel_gap, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region, int _precision_bits, boolean _measure_row_gap_in_pixels) throws KduException;
  public boolean Process(int[] _buffer, int[] _channel_offsets, int _pixel_gap, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region) throws KduException
  {
    return Process(_buffer,_channel_offsets,_pixel_gap,_buffer_origin,_row_gap,_suggested_increment,_max_region_pixels,_incomplete_region,_new_region,(int) 16,(boolean) true);
  }
  public boolean Process(int[] _buffer, int[] _channel_offsets, int _pixel_gap, Kdu_coords _buffer_origin, int _row_gap, int _suggested_increment, int _max_region_pixels, Kdu_dims _incomplete_region, Kdu_dims _new_region, int _precision_bits) throws KduException
  {
    return Process(_buffer,_channel_offsets,_pixel_gap,_buffer_origin,_row_gap,_suggested_increment,_max_region_pixels,_incomplete_region,_new_region,_precision_bits,(boolean) true);
  }
  public native boolean Finish() throws KduException;
}
