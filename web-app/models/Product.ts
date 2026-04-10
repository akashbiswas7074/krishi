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
  fields: {
    type: [String],
    default: [],
  },
  imageUrl: {
    type: String,
    default: null,
  }
}, {
  timestamps: true
});

const Product: Model<IProduct> = mongoose.models.Product || mongoose.model<IProduct>('Product', ProductSchema);

export default Product;
