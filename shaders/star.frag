
        uniform float time;
        uniform vec3 colorA;
        uniform vec3 colorB;
        uniform vec3 colorC;
        uniform float wireframeOpacity;
        uniform float pulseSpeed;
        varying vec2 vUv;
        varying vec3 vPosition;
        varying vec3 vNormal;
        void main() {
          float radius = length(vPosition.xz);
          float gradient = smoothstep(0.5, 3.0, radius);
          float pulse = sin(time * pulseSpeed * 2.0) * 0.3 + 0.7;
          vec3 baseColor = mix(colorA, colorB, gradient);
          float noise = sin(vPosition.x * 15.0 + time) * cos(vPosition.z * 15.0 + time) * 0.2;
          vec3 finalColor = mix(baseColor, colorC, noise);
          float fresnel = pow(1.0 - dot(normalize(cameraPosition - vPosition), vNormal), 3.0);
          finalColor += vec3(1.0) * fresnel * pulse;
          float energyFlow = sin(radius * 10.0 - time * 2.0) * 0.5 + 0.5;
          finalColor *= (1.0 + energyFlow * 0.3);
          gl_FragColor = vec4(finalColor, wireframeOpacity);
        }
      