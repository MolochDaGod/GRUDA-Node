/**
 * BLE Voice Service — connects to GRUDA Node via Bluetooth LE
 *
 * Scans for devices advertising as "GRUDA-Node-*", connects,
 * and provides methods to send transcribed text and receive AI responses.
 *
 * Uses Nordic UART Service (NUS) UUIDs matching the ESP32 firmware.
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

import { BleManager, Device, Characteristic, State } from 'react-native-ble-plx';
import { Buffer } from 'buffer';

/* UUIDs matching include/config.h */
const SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const RX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // phone → ESP32
const TX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // ESP32 → phone

const DEVICE_NAME_PREFIX = 'GRUDA-Node-';

export type BLEStatus = 'disconnected' | 'scanning' | 'connecting' | 'connected';

export interface AIResponse {
  type: string;
  text: string;
  ts: number;
}

export type ResponseCallback = (response: AIResponse) => void;

class BLEVoiceService {
  private manager: BleManager;
  private device: Device | null = null;
  private status: BLEStatus = 'disconnected';
  private responseCallbacks: ResponseCallback[] = [];
  private statusCallbacks: ((status: BLEStatus) => void)[] = [];

  constructor() {
    this.manager = new BleManager();
  }

  getStatus(): BLEStatus {
    return this.status;
  }

  getDeviceName(): string | null {
    return this.device?.name ?? null;
  }

  onResponse(cb: ResponseCallback): () => void {
    this.responseCallbacks.push(cb);
    return () => {
      this.responseCallbacks = this.responseCallbacks.filter((c) => c !== cb);
    };
  }

  onStatusChange(cb: (status: BLEStatus) => void): () => void {
    this.statusCallbacks.push(cb);
    return () => {
      this.statusCallbacks = this.statusCallbacks.filter((c) => c !== cb);
    };
  }

  private setStatus(s: BLEStatus) {
    this.status = s;
    this.statusCallbacks.forEach((cb) => cb(s));
  }

  /** Scan for GRUDA Node devices and connect to the first one found */
  async scanAndConnect(timeoutMs = 10000): Promise<boolean> {
    const state = await this.manager.state();
    if (state !== State.PoweredOn) {
      console.warn('[BLE] Bluetooth not powered on:', state);
      return false;
    }

    this.setStatus('scanning');

    return new Promise<boolean>((resolve) => {
      const timeout = setTimeout(() => {
        this.manager.stopDeviceScan();
        if (this.status === 'scanning') {
          this.setStatus('disconnected');
        }
        resolve(false);
      }, timeoutMs);

      this.manager.startDeviceScan(
        [SERVICE_UUID],
        { allowDuplicates: false },
        async (error, scannedDevice) => {
          if (error) {
            console.error('[BLE] Scan error:', error.message);
            return;
          }

          if (!scannedDevice?.name?.startsWith(DEVICE_NAME_PREFIX)) return;

          console.log('[BLE] Found:', scannedDevice.name);
          clearTimeout(timeout);
          this.manager.stopDeviceScan();

          try {
            this.setStatus('connecting');
            const connected = await scannedDevice.connect({ requestMTU: 512 });
            await connected.discoverAllServicesAndCharacteristics();
            this.device = connected;

            /* Monitor TX characteristic for AI responses from ESP32 */
            this.device.monitorCharacteristicForService(
              SERVICE_UUID,
              TX_CHAR_UUID,
              (err, char) => {
                if (err) {
                  console.error('[BLE] TX monitor error:', err.message);
                  return;
                }
                if (char?.value) {
                  const decoded = Buffer.from(char.value, 'base64').toString('utf-8');
                  try {
                    const parsed: AIResponse = JSON.parse(decoded);
                    this.responseCallbacks.forEach((cb) => cb(parsed));
                  } catch {
                    /* Plain text response */
                    this.responseCallbacks.forEach((cb) =>
                      cb({ type: 'text', text: decoded, ts: Date.now() })
                    );
                  }
                }
              }
            );

            /* Monitor disconnection */
            this.device.onDisconnected(() => {
              console.log('[BLE] Device disconnected');
              this.device = null;
              this.setStatus('disconnected');
            });

            this.setStatus('connected');
            console.log('[BLE] Connected to', scannedDevice.name);
            resolve(true);
          } catch (connErr: any) {
            console.error('[BLE] Connection failed:', connErr.message);
            this.setStatus('disconnected');
            resolve(false);
          }
        }
      );
    });
  }

  /** Send transcribed voice text to the GRUDA Node */
  async sendText(text: string): Promise<boolean> {
    if (!this.device || this.status !== 'connected') {
      console.warn('[BLE] Not connected');
      return false;
    }

    try {
      const encoded = Buffer.from(text, 'utf-8').toString('base64');
      await this.device.writeCharacteristicWithResponseForService(
        SERVICE_UUID,
        RX_CHAR_UUID,
        encoded
      );
      console.log('[BLE] Sent:', text);
      return true;
    } catch (err: any) {
      console.error('[BLE] Send failed:', err.message);
      return false;
    }
  }

  /** Disconnect from the current device */
  async disconnect(): Promise<void> {
    if (this.device) {
      try {
        await this.device.cancelConnection();
      } catch {
        /* already disconnected */
      }
      this.device = null;
    }
    this.setStatus('disconnected');
  }

  /** Clean up the BLE manager */
  destroy() {
    this.disconnect();
    this.manager.destroy();
  }
}

/* Singleton instance */
export const bleVoice = new BLEVoiceService();
