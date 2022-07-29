package kdu_jni;

public class Jp2_input_box extends Kdu_compressed_source {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  protected Jp2_input_box(long ptr) {
    super(ptr);
  }
  public native void Native_destroy();
  public void finalize() {
    if ((_native_ptr & 1) != 0)
      { // Resource created and not donated
        Native_destroy();
      }
  }
  private static native long Native_create();
  public Jp2_input_box() {
    this(Native_create());
  }
  public native boolean Exists() throws KduException;
  public native boolean Open(Jp2_family_src _src, Jp2_locator _locator) throws KduException;
  public native boolean Open(Jp2_input_box _super_box) throws KduException;
  public native boolean Open_next() throws KduException;
  public native void Transplant(Jp2_input_box _src) throws KduException;
  public native boolean Has_caching_source() throws KduException;
  public native long Get_box_type() throws KduException;
  public native Jp2_locator Get_locator() throws KduException;
  public native int Get_box_header_length() throws KduException;
  public native long Get_remaining_bytes() throws KduException;
  public native long Get_box_bytes() throws KduException;
  public native boolean Is_complete() throws KduException;
  public native boolean Load_in_memory(int _max_bytes) throws KduException;
  public native boolean Read(long[] _dword) throws KduException;
  public native boolean Read(int[] _dword) throws KduException;
  public native boolean Read(short[] _word) throws KduException;
  public native boolean Read(byte[] _byte) throws KduException;
  public native boolean Set_codestream_scope(long _codestream_id, boolean _need_main_header) throws KduException;
  public boolean Set_codestream_scope(long _codestream_id) throws KduException
  {
    return Set_codestream_scope(_codestream_id,(boolean) true);
  }
}
