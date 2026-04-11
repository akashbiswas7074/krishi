const XLSX = require('xlsx');
const path = require('path');

const filePath = path.join(__dirname, '../public/Budget_Terget.xlsx');
const workbook = XLSX.readFile(filePath);
const sheetName = workbook.SheetNames[0];
const sheet = workbook.Sheets[sheetName];
const data = XLSX.utils.sheet_to_json(sheet);

console.log("Keys found in first row:", Object.keys(data[0]));
console.log("Values found in first row:", JSON.stringify(data[0], null, 2));
console.log("Values found in second row:", JSON.stringify(data[1], null, 2));
