const express = require('express');
const multer = require('multer');
const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');
const cors = require('cors');
const util = require('util');
const execPromise = util.promisify(exec);
const sharp = require('sharp');

const app = express();
const port = 3000;

app.use(cors());
app.use(express.static('public'));

const storage = multer.diskStorage({
  destination: function (req, file, cb) {
    const uploadDir = path.join(__dirname, 'test', 'images');
    if (!fs.existsSync(uploadDir)) {
      fs.mkdirSync(uploadDir, { recursive: true });
    }
    cb(null, uploadDir);
  },
  filename: function (req, file, cb) {
    cb(null, 'upload_' + Date.now() + path.extname(file.originalname));
  }
});

const upload = multer({ storage: storage });

function parseOutput(stdout) {
    let cpuTime = "N/A";
    let gpuTime = "N/A";
    let entropyBefore = "N/A";
    let entropyAfter = "N/A";

    const cpuMatch = stdout.match(/CPU UM processing:\s*([\d.]+)\s*ms/);
    if (cpuMatch) cpuTime = parseFloat(cpuMatch[1]);

    const gpuMatch = stdout.match(/GPU UM processing \(Kernel Launch\):\s*([\d.]+)\s*ms/);
    if (gpuMatch) gpuTime = parseFloat(gpuMatch[1]);

    const entBeforeMatch = stdout.match(/Entropy before UM:\s*([\d.]+)/);
    if (entBeforeMatch) entropyBefore = parseFloat(entBeforeMatch[1]);

    const entAfterMatch = stdout.match(/Entropy after UM \(GPU\):\s*([\d.]+)/);
    if (entAfterMatch) entropyAfter = parseFloat(entAfterMatch[1]);

    return { cpuTime, gpuTime, entropyBefore, entropyAfter, rawOutput: stdout };
}

app.post('/api/enhance', upload.single('image'), (req, res) => {
  if (!req.file) {
    return res.status(400).json({ error: 'No image uploaded' });
  }

  const n = req.body.n || 5;
  const lambda = req.body.lambda || 2.5;

  const inputPath = req.file.path;
  const outputPath = path.join(__dirname, 'test', 'images', 'output_' + Date.now() + '.png');

  const exePath = path.join(__dirname, 'um_ocl.exe');
  const command = `"${exePath}" "${inputPath}" "${outputPath}" ${n} ${lambda}`;

  exec(command, (error, stdout, stderr) => {
    if (error) {
      console.error(`Execution error: ${error}`);
      return res.status(500).json({ error: 'Failed to process image' });
    }

    const metrics = parseOutput(stdout);

    fs.readFile(outputPath, (err, data) => {
      if (err) {
        return res.status(500).json({ error: 'Failed to read output image' });
      }
      
      const base64Image = `data:image/png;base64,${data.toString('base64')}`;
      
      try {
        fs.unlinkSync(inputPath);
        fs.unlinkSync(outputPath);
      } catch(e) {}

      res.json({
        success: true,
        image: base64Image,
        metrics
      });
    });
  });
});

app.post('/api/benchmark', upload.single('image'), async (req, res) => {
  if (!req.file) {
    return res.status(400).json({ error: 'No image uploaded for benchmark' });
  }

  const resolutions = [512, 1024, 2048, 4096];
  const results = [];
  const exePath = path.join(__dirname, 'um_ocl.exe');
  const n = 5;
  const lambda = 1.5;

  try {
    for (const size of resolutions) {
      const resizedInput = path.join(__dirname, 'test', 'images', `bench_in_${size}.png`);
      const resizedOutput = path.join(__dirname, 'test', 'images', `bench_out_${size}.png`);

      // Resize image using sharp
      await sharp(req.file.path)
        .resize({ width: size, height: size, fit: 'fill' })
        .toFile(resizedInput);

      const command = `"${exePath}" "${resizedInput}" "${resizedOutput}" ${n} ${lambda}`;
      const { stdout } = await execPromise(command);
      
      const metrics = parseOutput(stdout);
      
      results.push({
        size: `${size}x${size}`,
        cpuTime: metrics.cpuTime,
        gpuTime: metrics.gpuTime
      });

      // Cleanup
      try {
        fs.unlinkSync(resizedInput);
        fs.unlinkSync(resizedOutput);
      } catch(e) {}
    }
    
    // Cleanup original upload
    try {
        fs.unlinkSync(req.file.path);
    } catch(e) {}

    res.json({ success: true, results });
  } catch (error) {
    console.error("Benchmark error", error);
    res.status(500).json({ error: 'Benchmark failed' });
  }
});

app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
});
