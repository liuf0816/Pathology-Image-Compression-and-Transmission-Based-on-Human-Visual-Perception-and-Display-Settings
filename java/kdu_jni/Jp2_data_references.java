package kdu_jni;

public class Jp2_data_references {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  public native boolean Exists() throws KduException;
  public native int Add_url(String _url, int _url_idx) throws KduException;
  public int Add_url(String _url) throws KduException
  {
    return Add_url(_url,(int) 0);
  }
  public native int Get_num_urls() throws KduException;
  public native int Find_url(String _url) throws KduException;
  public native String Get_url(int _idx) throws KduException;
}
