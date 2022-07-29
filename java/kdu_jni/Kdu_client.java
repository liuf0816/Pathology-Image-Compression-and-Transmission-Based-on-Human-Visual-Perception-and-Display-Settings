package kdu_jni;

public class Kdu_client extends Kdu_cache {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  protected Kdu_client(long ptr) {
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
  public Kdu_client() {
    this(Native_create());
  }
  public native void Install_context_translator(Kdu_client_translator _translator) throws KduException;
  public native void Connect(String _server, String _proxy, String _request, String _channel_transport, String _cache_dir) throws KduException;
  public native boolean Is_active() throws KduException;
  public native boolean Is_one_time_request() throws KduException;
  public native boolean Is_alive() throws KduException;
  public native boolean Is_idle() throws KduException;
  public native void Disconnect(boolean _keep_transport_open, int _timeout_milliseconds) throws KduException;
  public void Disconnect() throws KduException
  {
    Disconnect((boolean) false,(int) 2000);
  }
  public void Disconnect(boolean _keep_transport_open) throws KduException
  {
    Disconnect(_keep_transport_open,(int) 2000);
  }
  public native void Install_notifier(Kdu_client_notifier _notifier) throws KduException;
  public native String Get_status() throws KduException;
  public native boolean Post_window(Kdu_window _window) throws KduException;
  public native boolean Get_window_in_progress(Kdu_window _window) throws KduException;
  public native int Get_received_bytes() throws KduException;
}
