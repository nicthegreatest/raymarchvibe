
        varying vec2 vUv;
        varying vec3 vPosition;
        varying vec3 vNormal;
        uniform float time;
        uniform float pulseSpeed;
        void main() {
          vUv = uv;
          vPosition = position;
          vNormal = normal;
          float pulse = sin(time * pulseSpeed) * 0.1;
          vec3 newPosition = position + normal * pulse;
          gl_Position = projectionMatrix * modelViewMatrix * vec4(newPosition, 1.0);
        }
      