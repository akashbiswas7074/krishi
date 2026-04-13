const XLSX = require('xlsx');
const path = require('path');

const filePath = path.join('/home/akashbiswas7797/Desktop/projects/krisi/web-app/public/Budget_Terget.xlsx');
const workbook = XLSX.readFile(filePath);
const sheetName = workbook.SheetNames[0];
const sheet = workbook.Sheets[sheetName];
const data = XLSX.utils.sheet_to_json(sheet);

console.log(JSON.stringify(data.slice(0, 10), null, 2));
