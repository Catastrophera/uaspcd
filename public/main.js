document.addEventListener('DOMContentLoaded', () => {
    const dropZone = document.getElementById('drop-zone');
    const fileInput = document.getElementById('file-input');
    const loader = document.getElementById('loader');
    const resultSection = document.getElementById('result-section');
    
    // Slider elements
    const slider = document.getElementById('comparison-slider');
    const sliderBefore = document.querySelector('.slider-before');
    const sliderHandle = document.querySelector('.slider-handle');
    const imgBefore = document.getElementById('image-before');
    const imgAfter = document.getElementById('image-after');

    // Metrics elements
    const mGpu = document.getElementById('m-gpu');
    const mCpu = document.getElementById('m-cpu');
    const mSpeedup = document.getElementById('m-speedup');
    const mEntropy = document.getElementById('m-entropy');
    const terminalOutput = document.getElementById('terminal-output');

    // Control Elements
    const lambdaSlider = document.getElementById('lambda-slider');
    const lambdaVal = document.getElementById('lambda-val');
    const nSlider = document.getElementById('n-slider');
    const nVal = document.getElementById('n-val');

    lambdaSlider.addEventListener('input', (e) => lambdaVal.textContent = e.target.value);
    nSlider.addEventListener('input', (e) => nVal.textContent = e.target.value);

    // Benchmark Elements
    const btnBenchmark = document.getElementById('btn-benchmark');
    const benchModal = document.getElementById('bench-modal');
    const closeBtn = document.querySelector('.close-btn');
    const benchOutput = document.getElementById('bench-output');
    let chartInstance = null;

    closeBtn.addEventListener('click', () => benchModal.style.display = 'none');
    window.addEventListener('click', (e) => {
        if (e.target === benchModal) benchModal.style.display = 'none';
    });

    // Drag and drop styles
    dropZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropZone.classList.add('dragover');
    });

    dropZone.addEventListener('dragleave', () => {
        dropZone.classList.remove('dragover');
    });

    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropZone.classList.remove('dragover');
        if (e.dataTransfer.files.length) {
            fileInput.files = e.dataTransfer.files;
            handleFileUpload(e.dataTransfer.files[0]);
        }
    });

    fileInput.addEventListener('change', (e) => {
        if (e.target.files.length) {
            handleFileUpload(e.target.files[0]);
        }
    });

    function handleFileUpload(file) {
        if (!file.type.startsWith('image/')) {
            alert('Please upload an image file.');
            return;
        }

        // Setup before image instantly
        const reader = new FileReader();
        reader.onload = (e) => {
            imgBefore.src = e.target.result;
            imgAfter.src = e.target.result;
        };
        reader.readAsDataURL(file);

        // Upload and process
        dropZone.style.display = 'none';
        btnBenchmark.disabled = true;
        loader.style.display = 'block';
        resultSection.style.display = 'none';

        const formData = new FormData();
        formData.append('image', file);
        formData.append('lambda', lambdaSlider.value);
        formData.append('n', nSlider.value);

        fetch('/api/enhance', {
            method: 'POST',
            body: formData
        })
        .then(response => response.json())
        .then(data => {
            loader.style.display = 'none';
            btnBenchmark.disabled = false;
            
            if (data.error) {
                alert('Error: ' + data.error);
                dropZone.style.display = 'block';
                return;
            }

            // Display results
            resultSection.style.display = 'flex';
            imgAfter.src = data.image;

            // Update dimensions after display is flex
            setTimeout(() => {
                const wrapper = document.querySelector('.slider-wrapper');
                if (imgBefore.naturalWidth > 0) {
                    const ratio = imgBefore.naturalHeight / imgBefore.naturalWidth;
                    wrapper.style.height = Math.min(800, wrapper.offsetWidth * ratio) + 'px';
                    imgBefore.style.width = wrapper.offsetWidth + 'px';
                }
            }, 50);

            // Update Metrics
            mGpu.textContent = data.metrics.gpuTime !== 'N/A' ? data.metrics.gpuTime + ' ms' : 'N/A';
            mCpu.textContent = data.metrics.cpuTime !== 'N/A' ? data.metrics.cpuTime + ' ms' : 'N/A';
            
            if (data.metrics.cpuTime !== 'N/A' && data.metrics.gpuTime !== 'N/A') {
                const speedup = (parseFloat(data.metrics.cpuTime) / parseFloat(data.metrics.gpuTime)).toFixed(2);
                mSpeedup.textContent = speedup + 'x';
            }

            if (data.metrics.entropyBefore !== 'N/A' && data.metrics.entropyAfter !== 'N/A') {
                const delta = (parseFloat(data.metrics.entropyAfter) - parseFloat(data.metrics.entropyBefore)).toFixed(4);
                mEntropy.textContent = (delta >= 0 ? '+' : '') + delta;
            }

            terminalOutput.textContent = data.metrics.rawOutput;
            
            // Reset slider
            slider.value = 50;
            updateSlider(50);
            dropZone.style.display = 'block'; // Allow another upload
        })
        .catch(error => {
            console.error('Error:', error);
            alert('Failed to process image. Make sure server is running.');
            loader.style.display = 'none';
            dropZone.style.display = 'block';
            btnBenchmark.disabled = false;
        });
    }

    // Benchmark Logic
    btnBenchmark.addEventListener('click', () => {
        if (!fileInput.files.length) {
            alert("Please upload a base image first to run benchmark.");
            return;
        }

        benchModal.style.display = 'flex';
        benchOutput.textContent = "Running benchmark... Please wait up to 10 seconds.";
        
        const formData = new FormData();
        formData.append('image', fileInput.files[0]);

        fetch('/api/benchmark', {
            method: 'POST',
            body: formData
        })
        .then(r => r.json())
        .then(data => {
            if (data.error) {
                benchOutput.textContent = "Error: " + data.error;
                return;
            }
            
            benchOutput.textContent = JSON.stringify(data.results, null, 2);
            renderChart(data.results);
        })
        .catch(err => {
            benchOutput.textContent = "Error running benchmark: " + err.message;
        });
    });

    function renderChart(results) {
        const ctx = document.getElementById('benchChart').getContext('2d');
        if (chartInstance) {
            chartInstance.destroy();
        }

        chartInstance = new Chart(ctx, {
            type: 'line',
            data: {
                labels: results.map(r => r.size),
                datasets: [
                    {
                        label: 'CPU Time (ms)',
                        data: results.map(r => r.cpuTime),
                        borderColor: '#888',
                        backgroundColor: 'rgba(136, 136, 136, 0.2)',
                        tension: 0.4
                    },
                    {
                        label: 'GPU Time (ms)',
                        data: results.map(r => r.gpuTime),
                        borderColor: '#00f3ff',
                        backgroundColor: 'rgba(0, 243, 255, 0.2)',
                        tension: 0.4
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: { color: '#222' }
                    },
                    x: {
                        grid: { color: '#222' }
                    }
                },
                plugins: {
                    legend: { labels: { color: '#e0e0e0' } }
                }
            }
        });
    }

    // Slider logic
    slider.addEventListener('input', (e) => {
        updateSlider(e.target.value);
    });

    function updateSlider(percentage) {
        sliderBefore.style.width = `${percentage}%`;
        sliderHandle.style.left = `${percentage}%`;
    }

    // Handle window resize for slider images
    window.addEventListener('resize', () => {
        if(imgBefore.src) {
            const wrapper = document.querySelector('.slider-wrapper');
            imgBefore.style.width = wrapper.offsetWidth + 'px';
            const ratio = imgBefore.naturalHeight / imgBefore.naturalWidth;
            wrapper.style.height = Math.min(800, wrapper.offsetWidth * ratio) + 'px';
        }
    });
});
