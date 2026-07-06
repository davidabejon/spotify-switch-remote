# spotify-switch

> **AVISO LEGAL**
>
> Esta aplicación es un **proyecto homebrew no oficial de uso personal** y no está afiliada, respaldada ni aprobada por Spotify AB.
>
> **Lo que esta app NO hace:** no reproduce, transmite ni decodifica audio; no extrae ni elude contenido protegido por DRM; no simula ni reemplaza al cliente oficial de Spotify; no replica ninguna experiencia central de Spotify. Únicamente lee metadatos de reproducción y envía comandos de control a través de la Spotify Web API oficial, exactamente como está documentado.
>[https://developer.spotify.com/documentation/web-api](https://developer.spotify.com/documentation/web-api).
>

---

Homebrew para Nintendo Switch que muestra tu reproducción de Spotify en tiempo real — portada del álbum, información de la canción, detalles del artista y metadatos del álbum — todo desde el sofá.

Construido con [Plutonium](https://github.com/XorTroll/Plutonium) (SDL2) y la API Web de Spotify.

---

## Características

- Estado de reproducción en tiempo real (canción, artista, portada)
- Panel de artista — foto, géneros, seguidores, popularidad
- Panel de álbum — miniatura, tipo, año de lanzamiento, número de canciones, sello discográfico
- Controles de reproducción: play/pausa, anterior y siguiente
- Inicio de sesión OAuth 2.0 PKCE mediante código QR (las credenciales no se guardan en texto plano)
- Renovación automática del token en segundo plano
- Pantalla de "sin reproducción activa" cuando no hay nada sonando

## Controles

| Botón | Acción |
|---|---|
| **L / R** | Cambiar entre las pestañas Reproductor y Favoritos |
| **ZL / ZR** | Cambiar entre los paneles Artista y Cola |
| **A** | Reproducir / Pausar |
| **← / →** | Canción anterior / siguiente |
| **+** | Salir |

## Requisitos

- Una Nintendo Switch con CFW que soporte homebrew (p.ej. Atmosphere)
- Una cuenta de Spotify (gratuita o Premium)

No necesitas una cuenta de desarrollador de Spotify — la app incluye su propio client ID.

## Instalación

Descarga el último `spotify-switch.nro` desde la página de [Releases](../../releases), cópialo a `/switch/spotify-switch/` en tu tarjeta SD y ábrelo desde el Homebrew Menu.

## Compilar desde el código fuente

Si prefieres compilarlo tú mismo:

- [devkitPro](https://devkitpro.org/) con el grupo `switch-dev` instalado
- Paquetes de pacman: `switch-curl`, `switch-mbedtls`, `switch-sdl2`, `switch-sdl2_image`, `switch-sdl2_ttf`, `switch-sdl2_mixer`

```sh
git clone --recurse-submodules https://github.com/davidabejon/spotify-switch-remote
cd spotify-switch
export DEVKITPRO=/opt/devkitpro
make
```

## Primera ejecución

En el primer arranque la app muestra un código QR. Escanéalo con el móvil, inicia sesión en Spotify y concede los permisos. El token se guarda en la SD — los arranques siguientes omiten la pantalla de inicio de sesión.

## Estructura del proyecto

```
source/          Código fuente C++
include/         Cabeceras
libs/Plutonium/  Framework de interfaz (submódulo git)
```

## Licencia

MIT
