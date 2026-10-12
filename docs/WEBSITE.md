# [pet.] project page

The first-pass project landing page lives in `website/`. It links to the existing OS experience at **https://mindofaxd.com/pet**, explains USB firmware flashing, and reserves a short section for future 3D enclosure documentation.

## Edit and preview

- `website/index.html`: page copy, links, and flashing steps.
- `website/style.css`: responsive layout and PET palette.
- `website/main.js`: progressive copy buttons; commands remain readable without JavaScript.
- `website/assets/pet-os.png`: native PET home-screen capture, copied from the existing project render.
- `website/assets/fonts/` and `website/assets/licenses/`: project typefaces and SIL OFL notices.

From the project root:

```sh
python3 -m http.server 8876 --bind 127.0.0.1 --directory website
```

Open http://127.0.0.1:8876. No build step or package install is needed. Keep the flashing steps in the root README and website synchronized with the scripts.

## Publish on GitHub Pages

The intended repository is `vworping/pet`, with `[pet.]` as its display title. Once that repository exists and these files have been pushed to `main`:

1. Open **Settings → Pages → Build and deployment**.
2. Set **Source** to **GitHub Actions**.
3. Run **Publish PET page** from the Actions tab, or push a change under `website/` to `main`.
4. Use the deployed URL returned by GitHub. The expected default address is `https://vworping.github.io/pet/`; it is not a deployment confirmation.

The workflow at `.github/workflows/pages.yml` deploys only `website/`. Local backups, credentials, firmware build output, and source directories are not part of the website artifact. This workflow does not build or flash firmware.

GitHub's official setup reference: https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site
