for (const button of document.querySelectorAll('[data-copy]')) {
  if (!navigator.clipboard?.writeText) continue;
  button.hidden = false;
  button.addEventListener('click', async () => {
    const status = document.querySelector('#copy-status');
    try {
      await navigator.clipboard.writeText(button.parentElement.querySelector('code').textContent);
      button.textContent = 'Copied';
      status.textContent = `${button.getAttribute('aria-label').replace('Copy ', '')} copied.`;
    } catch {
      button.textContent = 'Select to copy';
      status.textContent = 'Clipboard unavailable. Select the command text and copy it manually.';
    }
    setTimeout(() => { button.textContent = 'Copy'; }, 2200);
  });
}
