import pygame

pygame.init()

SCALE = 10
WIDTH = 96
HEIGTH = 64


screen = pygame.display.set_mode((WIDTH * SCALE, HEIGTH * SCALE))

BLACK = (0, 0, 0)
RED = (255, 0, 0)

running = True

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    screen.fill(BLACK)

    pygame.draw.polygon(
    screen,
    RED,
    [
        (20 * SCALE, 22 * SCALE),
        (40 * SCALE, 27 * SCALE),
        (38 * SCALE, 34 * SCALE),
        (20 * SCALE, 32 * SCALE),
    ]
    )

    pygame.draw.polygon(
    screen,
    RED,
    [
        (56 * SCALE, 27 * SCALE),
        (76 * SCALE, 22 * SCALE),
        (76 * SCALE, 32 * SCALE),
        (58 * SCALE, 34 * SCALE),
    ]
    )
    pygame.display.flip()
pygame.quit()
