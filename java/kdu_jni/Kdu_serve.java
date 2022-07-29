package kdu_jni;

public class Kdu_serve {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  protected Kdu_serve(long ptr) {
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
  public Kdu_serve() {
    this(Native_create());
  }
  public native void Initialize(Kdu_serve_target _target, int _max_chunk_size, int _chunk_prefix_bytes, boolean _ignore_relevance_info) throws KduException;
  public void Initialize(Kdu_serve_target _target, int _max_chunk_size, int _chunk_prefix_bytes) throws KduException
  {
    Initialize(_target,_max_chunk_size,_chunk_prefix_bytes,(boolean) false);
  }
  public native void Destroy() throws KduException;
  public native void Set_window(Kdu_window _window) throws KduException;
  public native boolean Get_window(Kdu_window _window) throws KduException;
  public native void Augment_cache_model(int _databin_class, int _stream_min, int _stream_max, long _bin_id, int _available_bytes, int _available_packets) throws KduException;
  public void Augment_cache_model(int _databin_class, int _stream_min, int _stream_max, long _bin_id, int _available_bytes) throws KduException
  {
    Augment_cache_model(_databin_class,_stream_min,_stream_max,_bin_id,_available_bytes,(int) 0);
  }
  public native void Truncate_cache_model(int _databin_class, int _stream_min, int _stream_max, long _bin_id, int _available_bytes, int _available_packets) throws KduException;
  public void Truncate_cache_model(int _databin_class, int _stream_min, int _stream_max, long _bin_id, int _available_bytes) throws KduException
  {
    Truncate_cache_model(_databin_class,_stream_min,_stream_max,_bin_id,_available_bytes,(int) 0);
  }
  public native void Augment_cache_model(int _stream_min, int _stream_max, int _tmin, int _tmax, int _cmin, int _cmax, int _rmin, int _rmax, int _pmin, int _pmax, int _available_bytes, int _available_packets) throws KduException;
  public native void Truncate_cache_model(int _stream_min, int _stream_max, int _tmin, int _tmax, int _cmin, int _cmax, int _rmin, int _rmax, int _pmin, int _pmax, int _available_bytes, int _available_packets) throws KduException;
  public native boolean Get_image_done() throws KduException;
  public native int Push_extra_data(byte[] _data, int _num_bytes) throws KduException;
}
