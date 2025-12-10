// Quick test to verify Express routes are working
const express = require('express');
const app = express();

app.get('/api/test', (req, res) => {
  res.json({ ok: true, message: 'API routes working!' });
});

app.get('/', (req, res) => {
  res.send('Root route working!');
});

const server = app.listen(8099, () => {
  console.log('Test server running on http://localhost:8099');
  console.log('Try: curl http://localhost:8099/api/test');
});
