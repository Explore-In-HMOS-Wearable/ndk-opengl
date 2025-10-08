export interface RenderContext {
  updateTransformMatrix(eventType: number, mXAngle: number, mYAngle: number): void
}

export const moveLeft: (context: ESObject) => void;
export const moveRight: (context: ESObject) => void;
export const restartGame: (context: ESObject) => void;
export const getContext: (value: number) => ESObject;

/**
 * Registers a callback to be invoked when the game is over.
 * @param context - XComponent context
 * @param callback - Function called with final score when game ends
 */
export const setGameOverCallback: (context: ESObject, callback: (score: number) => void) => void;