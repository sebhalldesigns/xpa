// IPC bridge to native backend
// In development, this is a no-op stub
// In production, this talks to the native webview via custom protocol

const isNative = typeof window.__xpa_invoke === 'function';

export async function invoke(command, args = {}) {
  if (isNative) {
    return window.__xpa_invoke(command, args);
  }
  console.log(`[ipc stub] ${command}`, args);
  return null;
}

export function listen(event, callback) {
  if (isNative) {
    return window.__xpa_listen(event, callback);
  }
  console.log(`[ipc stub] listening for ${event}`);
  return () => {};
}