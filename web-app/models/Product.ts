import mongoose, { Document, Model, Schema } from 'mongoose';

export interface IProduct extends Document {
  id: string;
  name: string;
  crops: string;
  y25: number;
  y26: number;
  aspiration: number;
  fields: string[];
  imageUrl: string | null;
  isActive: boolean;
  ledPin: number;
  unit: string;
}

const ProductSchema: Schema = new mongoose.Schema({
  id: {
    type: String,
    required: true,
    unique: true,
  },
  name: {
    type: String,
    required: true,
  },
  crops: {
    type: String,
    required: true,
  },
  y25: {
    type: Number,
    required: true,
    default: 0,
  },
  y26: {
    type: Number,
    required: true,
    default: 0,
  },
  aspiration: {
    type: Number,
    required: true,
    default: 0,
  },
  imageUrl: {
    type: String,
    default: null,
  },
  isActive: {
    type: Boolean,
    default: false,
  },
  ledPin: {
    type: Number,
    default: 2,
  },
  unit: {
    type: String,
    default: 'Kg',
  }
}, {
  timestamps: true
});

if (mongoose.models.Product) {
  delete mongoose.models.Product;
}
const Product: Model<IProduct> = mongoose.model<IProduct>('Product', ProductSchema);

export default Product;
