document.addEventListener('DOMContentLoaded', () => {
    const uploadZone = document.getElementById('upload-zone');
    const fileInput = document.getElementById('file-input');
    const fileGrid = document.getElementById('file-grid');
    const toast = document.getElementById('toast');

    // Load initial files
    loadFiles();

    // Event Listeners for Drag and Drop
    uploadZone.addEventListener('click', () => fileInput.click());

    uploadZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadZone.classList.add('dragover');
    });

    uploadZone.addEventListener('dragleave', () => {
        uploadZone.classList.remove('dragover');
    });

    uploadZone.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadZone.classList.remove('dragover');
        if (e.dataTransfer.files.length) {
            uploadFile(e.dataTransfer.files[0]);
        }
    });

    fileInput.addEventListener('change', () => {
        if (fileInput.files.length) {
            uploadFile(fileInput.files[0]);
            fileInput.value = ''; // Reset
        }
    });

    function showToast(message, isError = false) {
        toast.textContent = message;
        toast.className = `toast ${isError ? 'error' : ''}`;
        setTimeout(() => {
            toast.classList.add('hidden');
        }, 3000);
    }

    function formatBytes(bytes, decimals = 2) {
        if (!+bytes) return '0 Bytes';
        const k = 1024;
        const dm = decimals < 0 ? 0 : decimals;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return `${parseFloat((bytes / Math.pow(k, i)).toFixed(dm))} ${sizes[i]}`;
    }

    function getFileIcon(filename) {
        // Return SVG based on simple extension check
        const ext = filename.split('.').pop().toLowerCase();
        let path = '';
        
        if (['png', 'jpg', 'jpeg', 'gif', 'svg'].includes(ext)) {
            path = '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="17 8 12 3 7 8"></polyline><line x1="12" y1="3" x2="12" y2="15"></line>'; // image icon replacement
            return `<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect><circle cx="8.5" cy="8.5" r="1.5"></circle><polyline points="21 15 16 10 5 21"></polyline></svg>`;
        } else if (['txt', 'md', 'csv'].includes(ext)) {
            return `<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline><line x1="16" y1="13" x2="8" y2="13"></line><line x1="16" y1="17" x2="8" y2="17"></line><polyline points="10 9 9 9 8 9"></polyline></svg>`;
        } else {
            return `<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path><polyline points="13 2 13 9 20 9"></polyline></svg>`;
        }
    }

    function renderFiles(files) {
        fileGrid.innerHTML = '';
        if (files.length === 0) {
            fileGrid.innerHTML = '<p style="color: var(--text-secondary); grid-column: 1/-1; text-align: center;">No files uploaded yet.</p>';
            return;
        }

        files.forEach(file => {
            const card = document.createElement('a');
            card.className = 'file-card';
            card.href = `/uploads/${file.name}`;
            card.target = '_blank';
            
            card.innerHTML = `
                <div class="file-icon">
                    ${getFileIcon(file.name)}
                </div>
                <div class="file-name" title="${file.name}">${file.name}</div>
                <div class="file-size">${formatBytes(file.size)}</div>
            `;
            fileGrid.appendChild(card);
        });
    }

    async function loadFiles() {
        try {
            const response = await fetch('/cgi-bin/list_files.sh');
            if (response.ok) {
                const files = await response.json();
                renderFiles(files);
            } else {
                console.error('Failed to load files');
            }
        } catch (error) {
            console.error('Error fetching files:', error);
        }
    }

    async function uploadFile(file) {
        const formData = new FormData();
        formData.append('file', file);

        try {
            const response = await fetch('/upload', {
                method: 'POST',
                body: formData
            });

            if (response.ok) {
                const text = await response.text();
                showToast(text);
                loadFiles(); // Refresh list
            } else {
                showToast(`Upload failed: ${response.statusText}`, true);
            }
        } catch (error) {
            showToast('Upload error: Network issue', true);
        }
    }
});
