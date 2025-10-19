{
  description = "Master of Files";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        cspecs = pkgs.stdenv.mkDerivation {
          pname = "cspecs";
          version = "0.1.0";

          src = pkgs.fetchFromGitHub {
            owner = "mumuki";
            repo = "cspec";
            rev = "043089d9c55871526f1d6728af4d0febdbe8c38a";
            sha256 = "sha256-iH/mBu9H36EdCTyll8dy5nzW/VdEBHSVzouFLbYOkl4=";
          };

          nativeBuildInputs = with pkgs; [
            gnumake
            gcc
          ];

          buildPhase = ''
            mkdir -p release/cspecs/
            gcc -c -fmessage-length=0 -fPIC -MMD -MP -MF"release/cspecs/cspec.d" -MT"release/cspecs/cspec.d" -o "release/cspecs/cspec.o" "cspecs/cspec.c"
            gcc -shared -o "release/libcspecs.so" release/cspecs/cspec.o
          '';

          installPhase = ''
            mkdir -p $out/lib $out/include/cspecs
            cp release/libcspecs.so $out/lib/
            cp cspecs/cspec.h $out/include/cspecs/
          '';

          meta = with pkgs.lib; {
            description = "C testing framework";
            homepage = "https://github.com/mumuki/cspec";
            license = licenses.mit;
            platforms = platforms.linux;
          };
        };

      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            bear
            gcc
            gdb
            gnumake
            nnd
            valgrind

            glibc
            openssl
            openssl.dev
            readline
            pkg-config

            cspecs

            # NOTE: Correr 'cd so-commons-library && make debug && make install'
          ];

          shellHook = ''
            echo "Master of Files"
            echo ""

            # Setup so-commons-library environment if it exists
            if [ -d "so-commons-library/src" ]; then
              export SO_COMMONS_PATH="$(pwd)/so-commons-library"
              export C_INCLUDE_PATH="$C_INCLUDE_PATH:$SO_COMMONS_PATH/src"
              export LIBRARY_PATH="$LIBRARY_PATH:$SO_COMMONS_PATH/src/build"
              export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$SO_COMMONS_PATH/src/build"

              if [ ! -f "$SO_COMMONS_PATH/src/build/libcommons.so" ]; then
                echo "Setup requerido:"
                echo "  cd so-commons-library && make debug"
                echo ""
              else
                echo "so-commons-library: Lista"
              fi
            else
              echo "Warning: so-commons-library no encontrada"
            fi
          '';

          NIX_CFLAGS_COMPILE = "-I${pkgs.openssl.dev}/include -I${cspecs}/include";
          NIX_LDFLAGS = "-L${pkgs.openssl}/lib -L${cspecs}/lib -lssl -lcrypto -lreadline -lpthread -lcspecs";

          LD_LIBRARY_PATH = "${pkgs.openssl}/lib:${cspecs}/lib";
        };
      }
    );
}
