import mongoose, { Document, Model, Schema } from 'mongoose';

export interface ISystemConfig extends Document {
  isSlideshowActive: boolean;
  focusedProductId: string | null;
}

const SystemConfigSchema: Schema = new mongoose.Schema({
  isSlideshowActive: { type: Boolean, default: true },
  focusedProductId: { type: String, default: null }
});

const SystemConfig: Model<ISystemConfig> = mongoose.models.SystemConfig || mongoose.model<ISystemConfig>('SystemConfig', SystemConfigSchema);
export default SystemConfig;
