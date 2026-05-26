#ifndef AUTOENCODER_H
#define AUTOENCODER_H

#include <Arduino.h>
#include <math.h>

class TinyAutoencoder {
public:
    TinyAutoencoder(
        const float* w1, int w1_rows, int w1_cols, const float* b1,
        const float* w2, int w2_rows, int w2_cols, const float* b2,
        const float* w3, int w3_rows, int w3_cols, const float* b3,
        const float* w4, int w4_rows, int w4_cols, const float* b4,
        float threshold,
        const float* minVals, const float* maxVals
    )
        : m_w1(w1), m_w1_rows(w1_rows), m_w1_cols(w1_cols), m_b1(b1)
        , m_w2(w2), m_w2_rows(w2_rows), m_w2_cols(w2_cols), m_b2(b2)
        , m_w3(w3), m_w3_rows(w3_rows), m_w3_cols(w3_cols), m_b3(b3)
        , m_w4(w4), m_w4_rows(w4_rows), m_w4_cols(w4_cols), m_b4(b4)
        , m_threshold(threshold), m_lastMSE(0)
        , m_minVals(minVals), m_maxVals(maxVals)
    {}

    float computeMSE(const float* rawInput) {
        float norm[6];
        for (int i = 0; i < 6; i++) {
            float range = m_maxVals[i] - m_minVals[i];
            norm[i] = (range > 0) ? (rawInput[i] - m_minVals[i]) / range : 0;
            if (norm[i] < 0) norm[i] = 0;
            if (norm[i] > 1) norm[i] = 1;
        }

        float h1[4], h2[2], h3[4], out[6];
        dense(norm,     6, m_w1, m_w1_cols, m_b1, h1);  relu(h1, 4);
        dense(h1,       4, m_w2, m_w2_cols, m_b2, h2);  relu(h2, 2);
        dense(h2,       2, m_w3, m_w3_cols, m_b3, h3);  relu(h3, 4);
        dense(h3,       4, m_w4, m_w4_cols, m_b4, out);

        float mse = 0;
        for (int i = 0; i < 6; i++) {
            float diff = norm[i] - out[i];
            mse += diff * diff;
        }
        m_lastMSE = mse / 6.0f;
        return m_lastMSE;
    }

    bool isAnomaly(const float* input) {
        return computeMSE(input) > m_threshold;
    }

    float getLastMSE()    const { return m_lastMSE; }
    float getThreshold()  const { return m_threshold; }
    void  setThreshold(float t) { m_threshold = t; }

private:
    const float* m_w1, *m_b1, *m_w2, *m_b2, *m_w3, *m_b3, *m_w4, *m_b4;
    int m_w1_rows, m_w1_cols, m_w2_rows, m_w2_cols;
    int m_w3_rows, m_w3_cols, m_w4_rows, m_w4_cols;
    float m_threshold, m_lastMSE;
    const float* m_minVals;
    const float* m_maxVals;

    static void dense(const float* in, int inDim, const float* w, int outDim, const float* b, float* out) {
        for (int i = 0; i < outDim; i++) {
            out[i] = b[i];
            for (int j = 0; j < inDim; j++) {
                out[i] += in[j] * w[j * outDim + i];
            }
        }
    }

    static void relu(float* x, int n) {
        for (int i = 0; i < n; i++) {
            if (x[i] < 0) x[i] = 0;
        }
    }
};

#endif
