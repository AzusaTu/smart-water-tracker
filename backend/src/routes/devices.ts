import { Router } from 'express';
import {
  bindDevice,
  listDevices,
  rotateDeviceToken,
  unbindDevice,
  getDeviceStatus,
} from '../controllers/deviceController';
import { authenticateUser } from '../middleware/auth';

const router = Router();

router.use(authenticateUser);

router.post('/', bindDevice);
router.get('/', listDevices);
router.post('/:id/token/rotate', rotateDeviceToken);
router.delete('/:id', unbindDevice);
router.get('/:id/status', getDeviceStatus);

export default router;
