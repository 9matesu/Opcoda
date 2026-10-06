namespace OpcodaOtimizacao.Utilities;

public static class EntropiaShannon
{
    public static double BitsPorByte(ReadOnlySpan<byte> data)
    {
        if (data.Length == 0) return 0.0;

        Span<int> hist = stackalloc int[256];
        hist.Clear();
        foreach (var b in data) hist[b]++;

        double n = data.Length;
        double sum = 0.0;
        foreach (var occ in hist)
        {
            if (occ == 0) continue;
            double p = occ / n;
            sum += p * Math.Log2(p);
        }
        return -sum;
    }

    public static double ProporcaoZero(ReadOnlySpan<byte> data)
    {
        if (data.Length == 0) return 0.0;
        int z = 0;
        foreach (var b in data) if (b == 0x00) z++;
        return (double)z / data.Length;
    }
}