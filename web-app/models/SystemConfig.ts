import mongoose, { Document, Model, Schema } from 'mongoose';

export interface ISystemConfig extends Document {
  isSlideshowActive: boolean;
  focusedProductId: string | null;
  wifiSSID: string;
  wifiPass: string;
}

const SystemConfigSchema: Schema = new mongoose.Schema({
  isSlideshowActive: { type: Boolean, default: true },
  focusedProductId: { type: String, default: null },
  wifiSSID: { type: String, default: "Jharna's A15" },
  wifiPass: { type: String, default: "12345678" }
});

const SystemConfig: Model<ISystemConfig> = mongoose.models.SystemConfig || mongoose.model<ISystemConfig>('SystemConfig', SystemConfigSchema);
export default SystemConfig;
