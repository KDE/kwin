while read line; do
    echo $line
done

echo ""
echo "[Blacklist][Lanczos]"
echo "Intel=GM45 Express Chipset GEM 20100328:-:7.8.2,965GM GEM 20100328 2010Q1:-:7.8.2,965GM GEM 20091221 2009Q4:-:7.7.1"
echo "Advanced Micro Devices=DRI R600:-:7.8.1,DRI R600:-:7.8.2"
echo ""
echo "[Blacklist][Blur]"
echo "Advanced Micro Devices=DRI R600:-:7.8.1,DRI R600:-:7.8.2"
echo "Ati=Radeon HD 3650:-:3.3.9901"
echo "NVIDIA=GeForce 6150/PCI/SSE2:-:195"
echo ""