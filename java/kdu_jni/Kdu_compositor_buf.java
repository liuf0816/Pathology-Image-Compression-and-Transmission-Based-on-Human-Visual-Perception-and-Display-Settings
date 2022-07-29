package kdu_jni;

public class Kdu_compositor_buf {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  protected Kdu_compositor_buf(long ptr) {
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
  public Kdu_compositor_buf() {
    this(Native_create());
  }
  public native void Init(long _buf, int _row_gap) throws KduException;
  public native boolean Is_read_access_allowed() throws KduException;
  public native boolean Set_read_accessibility(boolean _read_access_required) throws KduException;
  public native long Get_buf(int[] _row_gap, boolean _read_write) throws KduException;
  public native void Get_region(Kdu_dims _src_region, int[] _tgt_buf, int _tgt_offset, int _tgt_row_gap) throws KduException;
  public void Get_region(Kdu_dims _src_region, int[] _tgt_buf) throws KduException
  {
    Get_region(_src_region,_tgt_buf,(int) 0,(int) 0);
  }
  public void Get_region(Kdu_dims _src_region, int[] _tgt_buf, int _tgt_offset) throws KduException
  {
    Get_region(_src_region,_tgt_buf,_tgt_offset,(int) 0);
  }
}
