package kdu_jni;

public class Kdu_region_compositor {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  protected Kdu_region_compositor(long ptr) {
    _native_ptr = ptr;
  }
  public native void Native_destroy();
  public void finalize() {
    if ((_native_ptr & 1) != 0)
      { // Resource created and not donated
        Native_destroy();
      }
  }
  private static native long Native_create(Kdu_thread_env _env, long _env_queue);
  private native void Native_init();
  public Kdu_region_compositor(Kdu_thread_env _env, long _env_queue) {
    this(Native_create(_env, _env_queue));
    this.Native_init();
  }
  private static long Native_create()
  {
    Kdu_thread_env env = null;
    return Native_create(env,0);
  }
  public Kdu_region_compositor() {
    this(Native_create());
    this.Native_init();
  }
  private static long Native_create(Kdu_thread_env _env)
  {
    return Native_create(_env,0);
  }
  public Kdu_region_compositor(Kdu_thread_env _env) {
    this(Native_create(_env));
    this.Native_init();
  }
  private static native long Native_create(Kdu_compressed_source _source, int _persistent_cache_threshold);
  public Kdu_region_compositor(Kdu_compressed_source _source, int _persistent_cache_threshold) {
    this(Native_create(_source, _persistent_cache_threshold));
    this.Native_init();
  }
  private static long Native_create(Kdu_compressed_source _source)
  {
    return Native_create(_source,(int) 256000);
  }
  public Kdu_region_compositor(Kdu_compressed_source _source) {
    this(Native_create(_source));
    this.Native_init();
  }
  private static native long Native_create(Jpx_source _source, int _persistent_cache_threshold);
  public Kdu_region_compositor(Jpx_source _source, int _persistent_cache_threshold) {
    this(Native_create(_source, _persistent_cache_threshold));
    this.Native_init();
  }
  private static long Native_create(Jpx_source _source)
  {
    return Native_create(_source,(int) 256000);
  }
  public Kdu_region_compositor(Jpx_source _source) {
    this(Native_create(_source));
    this.Native_init();
  }
  private static native long Native_create(Mj2_source _source, int _persistent_cache_threshold);
  public Kdu_region_compositor(Mj2_source _source, int _persistent_cache_threshold) {
    this(Native_create(_source, _persistent_cache_threshold));
    this.Native_init();
  }
  private static long Native_create(Mj2_source _source)
  {
    return Native_create(_source,(int) 256000);
  }
  public Kdu_region_compositor(Mj2_source _source) {
    this(Native_create(_source));
    this.Native_init();
  }
  public native void Pre_destroy() throws KduException;
  public native void Create(Kdu_compressed_source _source, int _persistent_cache_threshold) throws KduException;
  public void Create(Kdu_compressed_source _source) throws KduException
  {
    Create(_source,(int) 256000);
  }
  public native void Create(Jpx_source _source, int _persistent_cache_threshold) throws KduException;
  public void Create(Jpx_source _source) throws KduException
  {
    Create(_source,(int) 256000);
  }
  public native void Create(Mj2_source _source, int _persistent_cache_threshold) throws KduException;
  public void Create(Mj2_source _source) throws KduException
  {
    Create(_source,(int) 256000);
  }
  public native void Set_error_level(int _error_level) throws KduException;
  public native void Set_surface_initialization_mode(boolean _pre_initialize) throws KduException;
  public native boolean Add_compositing_layer(int _layer_idx, Kdu_dims _full_source_dims, Kdu_dims _full_target_dims, boolean _transpose, boolean _vflip, boolean _hflip, int _frame_idx, int _field_handling) throws KduException;
  public boolean Add_compositing_layer(int _layer_idx, Kdu_dims _full_source_dims, Kdu_dims _full_target_dims) throws KduException
  {
    return Add_compositing_layer(_layer_idx,_full_source_dims,_full_target_dims,(boolean) false,(boolean) false,(boolean) false,(int) 0,(int) 2);
  }
  public boolean Add_compositing_layer(int _layer_idx, Kdu_dims _full_source_dims, Kdu_dims _full_target_dims, boolean _transpose) throws KduException
  {
    return Add_compositing_layer(_layer_idx,_full_source_dims,_full_target_dims,_transpose,(boolean) false,(boolean) false,(int) 0,(int) 2);
  }
  public boolean Add_compositing_layer(int _layer_idx, Kdu_dims _full_source_dims, Kdu_dims _full_target_dims, boolean _transpose, boolean _vflip) throws KduException
  {
    return Add_compositing_layer(_layer_idx,_full_source_dims,_full_target_dims,_transpose,_vflip,(boolean) false,(int) 0,(int) 2);
  }
  public boolean Add_compositing_layer(int _layer_idx, Kdu_dims _full_source_dims, Kdu_dims _full_target_dims, boolean _transpose, boolean _vflip, boolean _hflip) throws KduException
  {
    return Add_compositing_layer(_layer_idx,_full_source_dims,_full_target_dims,_transpose,_vflip,_hflip,(int) 0,(int) 2);
  }
  public boolean Add_compositing_layer(int _layer_idx, Kdu_dims _full_source_dims, Kdu_dims _full_target_dims, boolean _transpose, boolean _vflip, boolean _hflip, int _frame_idx) throws KduException
  {
    return Add_compositing_layer(_layer_idx,_full_source_dims,_full_target_dims,_transpose,_vflip,_hflip,_frame_idx,(int) 2);
  }
  public native boolean Change_compositing_layer_frame(int _layer_idx, int _frame_idx) throws KduException;
  public native void Remove_compositing_layer(int _layer_idx, boolean _permanent) throws KduException;
  public native int Set_single_component(int _stream_idx, int _component_idx, int _access_mode) throws KduException;
  public native void Cull_inactive_layers(int _max_inactive) throws KduException;
  public native void Set_frame(Jpx_frame_expander _expander) throws KduException;
  public native void Set_scale(boolean _transpose, boolean _vflip, boolean _hflip, float _scale) throws KduException;
  public native float Find_optimal_scale(Kdu_dims _region, float _scale_anchor, float _min_scale, float _max_scale, int[] _compositing_layer_idx, int[] _codestream_idx, int[] _component_idx) throws KduException;
  public float Find_optimal_scale(Kdu_dims _region, float _scale_anchor, float _min_scale, float _max_scale) throws KduException
  {
    return Find_optimal_scale(_region,_scale_anchor,_min_scale,_max_scale,null,null,null);
  }
  public float Find_optimal_scale(Kdu_dims _region, float _scale_anchor, float _min_scale, float _max_scale, int[] _compositing_layer_idx) throws KduException
  {
    return Find_optimal_scale(_region,_scale_anchor,_min_scale,_max_scale,_compositing_layer_idx,null,null);
  }
  public float Find_optimal_scale(Kdu_dims _region, float _scale_anchor, float _min_scale, float _max_scale, int[] _compositing_layer_idx, int[] _codestream_idx) throws KduException
  {
    return Find_optimal_scale(_region,_scale_anchor,_min_scale,_max_scale,_compositing_layer_idx,_codestream_idx,null);
  }
  public native void Set_buffer_surface(Kdu_dims _region, int _background) throws KduException;
  public void Set_buffer_surface(Kdu_dims _region) throws KduException
  {
    Set_buffer_surface(_region,(int) -1);
  }
  public native int Check_invalid_scale_code() throws KduException;
  public native boolean Get_total_composition_dims(Kdu_dims _dims) throws KduException;
  public native Kdu_compositor_buf Get_composition_buffer(Kdu_dims _region) throws KduException;
  public native boolean Push_composition_buffer(long _stamp, int _id) throws KduException;
  public native boolean Pop_composition_buffer() throws KduException;
  public native boolean Inspect_composition_queue(int _elt, long[] _stamp, int[] _id) throws KduException;
  public boolean Inspect_composition_queue(int _elt) throws KduException
  {
    return Inspect_composition_queue(_elt,null,null);
  }
  public boolean Inspect_composition_queue(int _elt, long[] _stamp) throws KduException
  {
    return Inspect_composition_queue(_elt,_stamp,null);
  }
  public native void Flush_composition_queue() throws KduException;
  public native void Set_max_quality_layers(int _quality_layers) throws KduException;
  public native int Get_max_available_quality_layers() throws KduException;
  public native Kdu_thread_env Set_thread_env(Kdu_thread_env _env, long _env_queue) throws KduException;
  public native boolean Process(int _suggested_increment, Kdu_dims _new_region) throws KduException;
  public native boolean Is_processing_complete() throws KduException;
  public native boolean Refresh() throws KduException;
  public native void Halt_processing() throws KduException;
  public native boolean Find_point(Kdu_coords _point, int[] _layer_idx, int[] _codestream_idx) throws KduException;
  public native boolean Map_region(int[] _codestream_idx, Kdu_dims _region) throws KduException;
  public native Kdu_dims Inverse_map_region(Kdu_dims _region, int _codestream_idx, int _layer_idx) throws KduException;
  public Kdu_dims Inverse_map_region(Kdu_dims _region, int _codestream_idx) throws KduException
  {
    return Inverse_map_region(_region,_codestream_idx,(int) -1);
  }
  public native Kdu_dims Find_layer_region(int _layer_idx, int _instance, boolean _apply_cropping) throws KduException;
  public native Kdu_dims Find_codestream_region(int _codestream_idx, int _instance, boolean _apply_cropping) throws KduException;
  public native long Get_next_codestream(long _last_stream_ref, boolean _only_active_codestreams, boolean _no_duplicates) throws KduException;
  public native long Get_next_visible_codestream(long _last_stream_ref, Kdu_dims _region, boolean _include_alpha) throws KduException;
  public native Kdu_codestream Access_codestream(long _stream_ref) throws KduException;
  public native boolean Get_codestream_info(long _stream_ref, int[] _codestream_idx, int[] _compositing_layer_idx, int[] _components_in_use, int[] _principle_component_idx, float[] _principle_component_scale_x, float[] _principle_component_scale_y) throws KduException;
  public boolean Get_codestream_info(long _stream_ref, int[] _codestream_idx, int[] _compositing_layer_idx) throws KduException
  {
    return Get_codestream_info(_stream_ref,_codestream_idx,_compositing_layer_idx,null,null,null,null);
  }
  public boolean Get_codestream_info(long _stream_ref, int[] _codestream_idx, int[] _compositing_layer_idx, int[] _components_in_use) throws KduException
  {
    return Get_codestream_info(_stream_ref,_codestream_idx,_compositing_layer_idx,_components_in_use,null,null,null);
  }
  public boolean Get_codestream_info(long _stream_ref, int[] _codestream_idx, int[] _compositing_layer_idx, int[] _components_in_use, int[] _principle_component_idx) throws KduException
  {
    return Get_codestream_info(_stream_ref,_codestream_idx,_compositing_layer_idx,_components_in_use,_principle_component_idx,null,null);
  }
  public boolean Get_codestream_info(long _stream_ref, int[] _codestream_idx, int[] _compositing_layer_idx, int[] _components_in_use, int[] _principle_component_idx, float[] _principle_component_scale_x) throws KduException
  {
    return Get_codestream_info(_stream_ref,_codestream_idx,_compositing_layer_idx,_components_in_use,_principle_component_idx,_principle_component_scale_x,null);
  }
  public native boolean Get_codestream_packets(long _stream_ref, Kdu_dims _region, long[] _visible_precinct_samples, long[] _visible_packet_samples, long[] _max_visible_packet_samples) throws KduException;
  public native void Configure_overlays(boolean _enable, int _min_display_size, int _painting_param) throws KduException;
  public native void Update_overlays(boolean _start_from_scratch) throws KduException;
  public native Jpx_metanode Search_overlays(Kdu_coords _point, int[] _codestream_idx) throws KduException;
  public boolean Custom_paint_overlay(Kdu_compositor_buf _buffer, Kdu_dims _buffer_region, Kdu_dims _bounding_region, int _codestream_idx, Jpx_metanode _node, int _painting_param, Kdu_coords _image_offset, Kdu_coords _subsampling, boolean _transpose, boolean _vflip, boolean _hflip, Kdu_coords _expansion_numerator, Kdu_coords _expansion_denominator, Kdu_coords _compositing_offset) throws KduException
  {
    // Override in a derived class to respond to the callback
    return false;
  }
  public Kdu_compositor_buf Allocate_buffer(Kdu_coords _min_size, Kdu_coords _actual_size, boolean _read_access_required) throws KduException
  {
    // Override in a derived class to respond to the callback
    return null;
  }
  public void Delete_buffer(Kdu_compositor_buf _buf) throws KduException
  {
    // Override in a derived class to respond to the callback
    return;
  }
}
