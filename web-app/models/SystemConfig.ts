import mongoose, { Document, Model, Schema } from 'mongoose';

export interface ISystemConfig extends Document {
  isSlideshowActive: boolean;
  focusedProductId: string | null;
  wifiNetworks: { ssid: string, pass: string }[];
  activeWifiIndex: number;
}

const SystemConfigSchema: Schema = new mongoose.Schema({
  isSlideshowActive: { type: Boolean, default: true },
  focusedProductId: { type: String, default: null },
  wifiNetworks: { 
    type: [{ ssid: String, pass: String }], 
    default: [{ ssid: "Jharna's A15", pass: "12345678" }] 
  },
  activeWifiIndex: { type: Number, default: 0 }
});

const SystemConfig: Model<ISystemConfig> = mongoose.models.SystemConfig || mongoose.model<ISystemConfig>('SystemConfig', SystemConfigSchema);
export default SystemConfig;
