require('dotenv').config();
import dbConnect from '../lib/mongodb';
import User from '../models/User';

async function migrate() {
  await dbConnect();
  const users = await User.find({});
  console.log(`Found ${users.length} users.`);
  
  for (const user of users) {
    user.role = 'admin';
    await user.save();
    console.log(`Updated user ${user.username} to admin.`);
  }
  process.exit(0);
}

migrate();
